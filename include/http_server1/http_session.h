#include <utility>

#pragma once

#include "attr.h"
#include "http_utils.h"
#include "websocket_session.h"

namespace http_server {

    // Handles an HTTP server connection
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        // Take ownership of the socket
        HttpSession(tcp::socket socket, Attr &attr)
                : socket_(std::move(socket)), strand_(socket_.get_executor()), attr_(attr),
                  timer_(socket_.get_executor().context(), (std::chrono::steady_clock::time_point::max) ()) {
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

            this->do_read();
        }

        void do_read() {
            // Set the timer
//            timer_.expires_after(attr_.timeout);

            // Make the request empty before reading,
            // otherwise the operation behavior is undefined.
            req_ = {};

            // Read a request
            http::async_read(socket_, read_buffer_, req_,
                             asio::bind_executor(
                                     strand_,
                                     [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                                         // happens when the timer closes the socket
                                         if (ec == asio::error::operation_aborted) {
                                             return;
                                         }
                                         // This means they closed the connection
                                         if (ec == http::error::end_of_stream) {
                                             do_shutdown("read_end_of_stream");
                                             return;
                                         }
                                         if (ec) {
                                             Logger::error("HttpSession", "Http read data error, error_message={}.",
                                                           ec.message());
                                             return;
                                         }

                                         // See if it is a WebSocket Upgrade
                                         if (websocket::is_upgrade(req_)) {
                                             handle_websocket();
                                         } else {
                                             // Send the response
                                             handle_http_request();

                                             do_read();
                                         }
                                     }));
        }

        template<typename T>
        void do_write(std::shared_ptr<http::response<T>> resp) {
            this->resp_ = resp;
            http::async_write(
                    socket_,
                    *resp,
                    asio::bind_executor(
                            strand_,
                            [this, resp](boost::system::error_code ec, std::size_t bytes_transferred) {
                                // Happens when the timer closes the socket
                                if (ec == asio::error::operation_aborted) {
                                    return;
                                }

                                if (ec) {
                                    Logger::error("HttpSession", "Http write error,  error_message={}.", ec.message());
                                    return;
                                }
                                if (resp->need_eof()) {
                                    // This means we should close the connection, usually because
                                    // the response indicated the "Connection: close" semantic.
                                    do_shutdown("write_eof");
                                }
                            }));
        }

        void do_shutdown(std::string reason) {
            Logger::info("HttpSession", "Socket shutdown[{}], and remove http session, host={}, port={}.", reason,
                         socket_.remote_endpoint().address().to_string(), socket_.remote_endpoint().port());
            // Send a TCP shutdown
            beast::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            // At this point the connection is closed gracefully
            std::lock_guard<std::mutex> locker(attr_.http_mutex);
            attr_.http_sessions.erase(shared_from_this());
        }

        HttpStatus handle_static() {
            if (attr_.webroot.empty()) {
                return HttpStatus::not_found;
            }

            if ((req_.method() != http::verb::get) && (req_.method() != http::verb::head)) {
                return HttpStatus::not_found;
            }

            std::string path = HttpUtils::path_cat(attr_.webroot, req_.target().to_string());
            if (req_.target().back() == '/') {
                path.append(attr_.index_file);
            }

            boost::system::error_code ec;
            http::file_body::value_type body;
            body.open(path.data(), beast::file_mode::scan, ec);
            if (ec) {
                return HttpStatus::not_found;
            }

            std::uint64_t const size = body.size();
            FileResponsePtr resp = FileResponsePtr(new FileResponse{
                    std::piecewise_construct,
                    std::make_tuple(std::move(body)),
                    std::make_tuple(attr_.http_headers)
            });
            resp->version(req_.version());
            resp->keep_alive(req_.keep_alive());
            resp->content_length(size);
            resp->set(HttpHeader::content_type, HttpUtils::mime_type(path));
            do_write(resp);
            return HttpStatus::ok;
        }

        HttpStatus handle_dynamic() {
            if (attr_.http_routes.empty()) {
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
            for (auto const &regex_method : attr_.http_routes) {
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
                        HttpResponsePtr resp = HttpResponsePtr(new HttpResponse());
                        try {
                            HttpHandler const &http_handler = match->second;
                            // handle biz
                            http_handler(req_, *resp);

                            resp->version(req_.version());
                            resp->keep_alive(req_.keep_alive());
                            resp->content_length(resp->body().size());

                            do_write(std::move(resp));

                            return HttpStatus::ok;
                        }
                        catch (std::exception &e) {
                            Logger::error("HttpSession", "Handle http error,  error_message={}.", e.what());
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
            do_write(res);
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

        void handle_websocket() {
            WebsocketSessionPtr session(new WebsocketSession(std::move(socket_), attr_, std::move(req_)));
            std::lock_guard<std::mutex> locker(attr_.websocket_mutex);
            attr_.websocket_sessions.insert(session);
            Logger::info("HttpSession", "Create new websocket session, session_id={}.", session->session_id());

            session->do_accept();
        }

    private:
        tcp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        beast::flat_buffer read_buffer_;
        Attr &attr_;
        HttpRequest req_;
        std::shared_ptr<void> resp_;
    };
}