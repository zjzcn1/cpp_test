#pragma once

#include "common.h"
#include "http_utils.h"
#include "ws_session.h"

namespace http_server {

// Handles an HTTP server connection
    class HttpSession : public std::enable_shared_from_this<HttpSession> {

    public:
        // Take ownership of the socket
        HttpSession(tcp::socket socket, Attr &attr)
                : socket_(std::move(socket)),
                  timer_(socket.get_executor().context(), (std::chrono::steady_clock::time_point::max) ()),
                  strand_(socket_.get_executor()),
                  attr_(attr) {
        }

        // Start the asynchronous operation
        void run() {
            // Make sure we run on the strand
            if(! strand_.running_in_this_thread())
                return asio::post(
                        asio::bind_executor(
                                strand_,
                                std::bind(
                                        &HttpSession::run,
                                        shared_from_this())));

            this->do_timer();
            this->do_read();
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
                                    std::placeholders::_2,
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
                                    std::placeholders::_2,
                                    res->need_eof())));
        }

        Status handle_static() {
            if (attr_.web_dir.empty()) {
                return Status::not_found;
            }

            if ((req_.method() != http::verb::get) && (req_.method() != http::verb::head)) {
                return Status::not_found;
            }

            std::string path = HttpUtils::path_cat(attr_.web_dir, req_.target().to_string());
            if (req_.target().back() == '/') {
                path.append(attr_.index_file);
            }

            boost::system::error_code ec;
            http::file_body::value_type body;
            body.open(path.data(), beast::file_mode::scan, ec);
            if (ec) {
                return Status::not_found;
            }

            auto const size = body.size();
            FileResponsePtr res = FileResponsePtr(new FileResponse{
                    std::piecewise_construct,
                    std::make_tuple(std::move(body)),
                    std::make_tuple(attr_.http_headers)
            });
            res->version(req_.version());
            res->keep_alive(req_.keep_alive());
            res->content_length(size);
            res->set(Header::content_type, HttpUtils::mime_type(path));
            send_file(res);
            return Status::ok;
        }

        Status handle_dynamic() {
            if (attr_.http_routes.empty()) {
                return Status::not_found;
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

            // iterate over routes
            for (auto const &regex_method : attr_.http_routes) {
                bool method_match{false};
                auto match = regex_method.second.find(static_cast<int>(Method::unknown));

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
                            auto const &http_handler = match->second;
                            // handle biz
                            http_handler(req_, *res);

                            res->version(req_.version());
                            res->keep_alive(req_.keep_alive());
                            res->content_length(res->body().size());

                            send_http(std::move(res));

                            return Status::ok;
                        }
                        catch (...) {
                            return Status::internal_server_error;
                        }
                    }
                }
            }

            return Status::not_found;
        }

        void handle_error(Status err) {
            HttpResponsePtr res = HttpResponsePtr(new HttpResponse());
            res->version(req_.version());
            res->keep_alive(req_.keep_alive());
            res->result(err);
            res->set(Header::content_type, "text/plain");
            res->body() = "Error: " + std::to_string(res->result_int());
            res->content_length(res->body().size());
            send_http(res);
        };

        void handle_request() {
            if (req_.target().empty()) {
                req_.target() = "/";
            }

            if (req_.target().at(0) != '/' ||
                req_.target().find("..") != boost::beast::string_view::npos) {
                this->handle_error(Status::not_found);
                return;
            }

            // serve dynamic content
            auto dyna = this->handle_dynamic();
            // success
            if (dyna == Status::ok) {
                return;
            }
            // error
            if (dyna != Status::not_found) {
                this->handle_error(dyna);
                return;
            }

            // serve static content
            auto stat = this->handle_static();
            if (stat != Status::ok) {
                this->handle_error(stat);
                return;
            }
        }

        void do_read() {
            timer_.expires_after(attr_.timeout);

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
                                             std::placeholders::_1,
                                             std::placeholders::_2)));
        }

        void on_read(beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            // the timer has closed the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }

            // This means they closed the connection
            if (ec == http::error::end_of_stream) {
                this->do_shutdown();
            }

            if (ec) {
                return fail_log(ec, "on_read");
            }

            // check for websocket upgrade
            if (websocket::is_upgrade(req_)) {
                if (!attr_.support_websocket || attr_.websocket_routes.empty()) {
                    this->do_shutdown();
                    return;
                }

                // upgrade to websocket
                if (handle_websocket()) {
                    this->cancel_timer();
                    return;
                } else {
                    this->do_shutdown();
                    return;
                }
            }

            // Send the response
            this->handle_request();

            this->do_read()
        }

        void on_write(
                beast::error_code ec,
                std::size_t bytes_transferred,
                bool close) {
            boost::ignore_unused(bytes_transferred);

            if (ec)
                return fail_log(ec, "write");

            if (close) {
                // This means we should close the connection, usually because
                // the response indicated the "HttpSession: close" semantic.
                return do_close();
            }
            res_ = nullptr;
            // Read another request
            do_read();
        }

        void do_close() {
            // Send a TCP shutdown
            beast::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_send, ec);

            // At this point the connection is closed gracefully
        }

        //////////////////////////////////websocket/////////////////////////////////////
        bool handle_websocket() {
            // the request path
            std::string path{req_.target().to_string()};

            // separate the query parameters
            auto params = HttpUtils::split(path, "?", 1);
            path = params.at(0);

            // regex variables
            std::smatch rx_match{};
            std::regex_constants::syntax_option_type const rx_opts{std::regex::ECMAScript};
            std::regex_constants::match_flag_type const rx_flgs{std::regex_constants::match_not_null};

            // check for matching route
            for (auto const &route_item : attr_.websocket_routes) {
                std::regex rx_str{route_item.first, rx_opts};

                if (std::regex_match(path, rx_match, rx_str, rx_flgs)) {
                    // create websocket
                    WsSessionPtr ws_session(
                            new WsSession(std::move(socket_), attr_, std::move(req_), route_item.second));
                    ws_session->run();

                    return true;
                }
            }

            return false;
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

    private:
        tcp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        beast::flat_buffer buffer_;
        asio::steady_timer timer_;
        Attr &attr_;
        HttpRequest req_;
        std::shared_ptr<void> res_;
    };

    typedef std::shared_ptr<HttpSession> HttpSessionPtr;
}