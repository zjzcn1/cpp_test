#pragma once

#include "common.h"
#include "http_utils.h"

namespace http_server {

// Handles an HTTP server connection
    class Session : public std::enable_shared_from_this<Session> {

    public:
        // Take ownership of the socket
        Session(tcp::socket socket, Attr &attr)
                : socket_(std::move(socket)), strand_(socket_.get_executor()), attr_(attr) {
        }

        // Start the asynchronous operation
        void run() {
            do_read();
        }

        void send_http(HttpResponsePtr res) {
            this->res_ = res;
            http::async_write(
                    socket_,
                    *res,
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &Session::on_write,
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
                                    &Session::on_write,
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
            if(req_.target().back() == '/') {
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
                            http_handler(req_, res);

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
            // Make the request empty before reading,
            // otherwise the operation behavior is undefined.
            req_ = {};

            // Read a request
            http::async_read(socket_, buffer_, req_,
                             asio::bind_executor(
                                     strand_,
                                     std::bind(
                                             &Session::on_read,
                                             shared_from_this(),
                                             std::placeholders::_1,
                                             std::placeholders::_2)));
        }

        void on_read(
                beast::error_code ec,
                std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            // This means they closed the connection
            if (ec == http::error::end_of_stream)
                return do_close();

            if (ec)
                return fail_log(ec, "read");

            // Send the response
            handle_request();
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
                // the response indicated the "Session: close" semantic.
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

    private:
        tcp::socket socket_;
        asio::strand <asio::io_context::executor_type> strand_;
        beast::flat_buffer buffer_;
        Attr &attr_;
        HttpRequest req_;
        std::shared_ptr<void> res_;
    };

    typedef std::shared_ptr<Session> SessionPtr;
}