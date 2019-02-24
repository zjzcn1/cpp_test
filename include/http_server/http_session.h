#pragma once

#include "common.h"
#include "http_utils.h"
#include "websocket_session.h"

namespace http_server {

    // Handles an HTTP server connection
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    private:
        tcp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        beast::flat_buffer buffer_;
        HttpConfig &http_config_;
        HttpRequest req_;
        std::shared_ptr<void> res_;

    public:
        // Take ownership of the socket
        explicit HttpSession(tcp::socket socket, HttpConfig &http_config)
                : socket_(std::move(socket)), strand_(socket_.get_executor()),
                  timer_(socket_.get_executor().context(), (std::chrono::steady_clock::time_point::max) ()),
                  http_config_(http_config) {
        }

        // Start the asynchronous operation
        void run() {
            // Make sure we run on the strand
            if (!strand_.running_in_this_thread())
                return asio::post(
                        asio::bind_executor(
                                strand_,
                                std::bind(
                                        &HttpSession::run,
                                        shared_from_this())));

            // Run the timer. The timer is operated
            // continuously, this simplifies the code.
            this->do_timer();
            this->do_read();
        }

        void do_read() {
            // Set the timer
            timer_.expires_after(std::chrono::seconds(http_config_.timeout));

            // Make the request empty before reading,
            // otherwise the operation behavior is undefined.
            req_ = {};

            // Read a request
            http::async_read(socket_, buffer_, req_,
                             asio::bind_executor(
                                     strand_,
                                     std::bind(
                                             &HttpSession::on_read,
                                             shared_from_this(),
                                             std::placeholders::_1, std::placeholders::_2)));
        }

        void on_read(beast::error_code ec, std::size_t bytes_) {
            boost::ignore_unused(bytes_);
            // happens when the timer closes the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }
            // This means they closed the connection
            if (ec == http::error::end_of_stream) {
                do_close();
                return;
            }

            if (ec){
                Logger::error("HttpSession on_read error,  error_message={}.", ec.message());
                return;
            }

            // See if it is a WebSocket Upgrade
            if (websocket::is_upgrade(req_)) {
                // Make timer expire immediately, by setting expiry to time_point::min we can detect
                // the upgrade to websocket in the timer handler
                timer_.expires_at((std::chrono::steady_clock::time_point::min) ());

                // Create a WebSocket websocket_session by transferring the socket
                if (handle_websocket()) {
                    this->cancel_timer();
                    return;
                } else {
                    this->do_shutdown();
                    return;
                }
            }

            // Send the response
            handle_http_request();

            do_read();
        }

        void on_write(beast::error_code ec, bool close) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }

            if (ec) {
                Logger::error("HttpSession on_write error,  error_message={}.", ec.message());
                return;
            }

            if (close) {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                return do_close();
            }
        }

        void do_close() {
            // Send a TCP shutdown
            beast::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_send, ec);
            // At this point the connection is closed gracefully
        }


        void cancel_timer() {
            // set the timer to expire immediately
            timer_.expires_at((std::chrono::steady_clock::time_point::min) ());
        }

        void do_timer() {
            // wait on the timer
            timer_.async_wait(
                    asio::bind_executor(strand_,
                                        std::bind(&HttpSession::on_timer, shared_from_this(), std::placeholders::_1)));
        }

        void on_timer(beast::error_code ec_ = {}) {
            if (ec_ && ec_ != asio::error::operation_aborted) {
                // TODO log here
                return;
            }

            // check if socket has been upgraded or closed
            if (timer_.expires_at() == (std::chrono::steady_clock::time_point::min) ()) {
                return;
            }

            // check expiry
            if (timer_.expiry() <= std::chrono::steady_clock::now()) {
                this->do_timeout();
                return;
            }
        }

        void do_timeout() {
            this->do_shutdown();
        }

        void do_shutdown() {
            beast::error_code ec;

            // send a tcp shutdown
            socket_.shutdown(tcp::socket::shutdown_send, ec);

            this->cancel_timer();

            if (ec) {
                // TODO log here
                return;
            }
        }

        void send_http(HttpResponsePtr res) {
            this->res_ = res;
            http::async_write(
                    socket_,
                    *res,
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &HttpSession::on_write,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    res->need_eof())));
        }

        void send_file(FileResponsePtr res) {
            this->res_ = res;
            http::async_write(
                    socket_,
                    *res,
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &HttpSession::on_write,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    res->need_eof())));
        }

        HttpStatus handle_static() {
            if (http_config_.webroot.empty()) {
                return HttpStatus::not_found;
            }

            if ((req_.method() != http::verb::get) && (req_.method() != http::verb::head)) {
                return HttpStatus::not_found;
            }

            std::string path = HttpUtils::path_cat(http_config_.webroot, req_.target().to_string());
            if (req_.target().back() == '/') {
                path.append(http_config_.index_file);
            }

            boost::system::error_code ec;
            http::file_body::value_type body;
            body.open(path.data(), beast::file_mode::scan, ec);
            if (ec) {
                return HttpStatus::not_found;
            }

            auto const size = body.size();
            FileResponsePtr res = FileResponsePtr(new FileResponse{
                    std::piecewise_construct,
                    std::make_tuple(std::move(body)),
                    std::make_tuple(http_config_.http_headers)
            });
            res->version(req_.version());
            res->keep_alive(req_.keep_alive());
            res->content_length(size);
            res->set(HttpHeader::content_type, HttpUtils::mime_type(path));
            send_file(res);
            return HttpStatus::ok;
        }

        HttpStatus handle_dynamic() {
            if (http_config_.http_routes.empty()) {
                return HttpStatus::not_found;
            }

            // regex variables
            std::smatch rx_match{};
            std::regex_constants::syntax_option_type const rx_opts{std::regex::ECMAScript};
            std::regex_constants::match_flag_type const rx_flgs{std::regex_constants::match_not_null};

            // the request path
            std::string path{req_.target().to_string()};

            // separate the query parameters
            auto params = HttpUtils::split(path, "?", 1);
            path = params.at(0);
            req_.path = path;
            HttpUtils::params_parse(path, req_.params);

            // iterate over routes
            for (auto const &regex_method : http_config_.http_routes) {
                bool method_match{false};
                auto match = regex_method.second.find(static_cast<int>(HttpMethod::unknown));

                if (match != regex_method.second.end()) {
                    method_match = true;
                } else {
                    match = regex_method.second.find(static_cast<int>(req_.method()));

                    if (match != regex_method.second.end()) {
                        method_match = true;
                    }
                }

                if (method_match) {
                    std::regex rx_str{regex_method.first, rx_opts};
                    if (std::regex_match(path, rx_match, rx_str, rx_flgs)) {
                        // set callback function
                        HttpResponsePtr res = HttpResponsePtr(new HttpResponse());
                        try {
                            HttpHandler const &http_handler = match->second;
                            // handle biz
                            http_handler(req_, *res);

                            res->version(req_.version());
                            res->keep_alive(req_.keep_alive());
                            res->content_length(res->body().size());

                            send_http(std::move(res));

                            return HttpStatus::ok;
                        }
                        catch (std::exception &e) {
                            Logger::error("HttpSession handle http error,  error_message={}.", e.what());
                            return HttpStatus::internal_server_error;
                        }
                    }
                }
            }

            return HttpStatus::not_found;
        }

        void handle_error(HttpStatus err) {
            HttpResponsePtr res = HttpResponsePtr(new HttpResponse());
            res->version(req_.version());
            res->keep_alive(req_.keep_alive());
            res->result(err);
            res->set(HttpHeader::content_type, "text/plain");
            res->body() = "Error: " + std::to_string(res->result_int());
            res->content_length(res->body().size());
            send_http(res);
        };

        void handle_http_request() {
            if (req_.target().empty()) {
                req_.target() = "/";
            }

            if (req_.target().at(0) != '/' ||
                req_.target().find("..") != boost::beast::string_view::npos) {
                this->handle_error(HttpStatus::not_found);
                return;
            }

            // serve dynamic content
            auto dyna = this->handle_dynamic();
            // success
            if (dyna == HttpStatus::ok) {
                return;
            }
            // error
            if (dyna != HttpStatus::not_found) {
                this->handle_error(dyna);
                return;
            }

            // serve static content
            HttpStatus status = this->handle_static();
            if (status != HttpStatus::ok) {
                this->handle_error(status);
                return;
            }
        }

        bool handle_websocket() {
            // the request path
            std::string path{req_.target().to_string()};

            // separate the query parameters
            std::vector<std::string> params = HttpUtils::split(path, "?", 1);
            path = params.at(0);
            req_.path = path;

            // regex variables
            std::smatch rx_match{};
            std::regex_constants::syntax_option_type const rx_opts{std::regex::ECMAScript};
            std::regex_constants::match_flag_type const rx_flgs{std::regex_constants::match_not_null};

            // check for matching route
            for (auto const &route_item : http_config_.websocket_routes) {
                std::regex rx_str{route_item.first, rx_opts};

                if (std::regex_match(path, rx_match, rx_str, rx_flgs)) {
                    std::make_shared<WebsocketSession>(std::move(socket_), http_config_, std::move(req_),route_item.second)
                            ->do_accept();
                    return true;
                }
            }

            return false;
        }
    };
}