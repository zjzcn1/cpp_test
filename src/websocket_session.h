#pragma once

#include "common.h"
#include "http_utils.h"

namespace http_server {

    class WebsocketSession;

    class Channel {
    public:

        Channel() = default;

        void insert(WebsocketSession &session) {
            sessions_.insert(&session);
        }

        void remove(WebsocketSession &session) {
            sessions_.erase(&session);
        }

//        void broadcast(std::string const &&str_) const {
//            for (auto const e : sessions_) {
//                e->send(std::move(str_));
//            }
//        }

        std::size_t size() const {
            return sessions_.size();
        }

    private:
        std::unordered_set<WebsocketSession *> sessions_;
    };

    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {

    public:
        WebsocketSession(tcp::socket socket, Attr &attr, HttpRequest &req, WebsocketHandler const &ws_handler)
                : ws_(std::move(socket)),
                  attr_(attr),
                  req_(std::move(req)),
                  ws_handler_{ws_handler},
                  strand_{socket.get_executor()} {
        }

        void send(std::string const &str_) {
            auto const pstr = std::make_shared<std::string const>(str_);
            que_.emplace_back(pstr);

            if (que_.size() > 1) {
                return;
            }

            ws_.async_write(asio::buffer(*que_.front()),
                            std::bind(&WebsocketSession::on_write, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
        }

        void do_accept() {
            ws_.control_callback(
                    std::bind(&WebsocketSession::on_control_callback, shared_from_this(), std::placeholders::_1,
                              std::placeholders::_2));

            ws_.async_accept(
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_accept,
                                    shared_from_this(),
                                    std::placeholders::_1)));
        }

        void on_accept(beast::error_code ec) {
            if (ec) {
                return fail_log(ec, "accept111");
            }

            this->do_read();
        }

        void on_control_callback(websocket::frame_type type_, boost::beast::string_view data_) {
            boost::ignore_unused(type_, data_);
        }

        void do_read() {
            ws_.async_read(
                    buffer_,
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_read,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2)));
        }

        void on_read(beast::error_code ec, std::size_t bytes) {
            boost::ignore_unused(bytes);
            // socket closed
            if (ec == websocket::error::closed) {
                return;
            }

            if (ec) {
                return fail_log(ec, "read");
            }

            try {
                std::string msg = beast::buffers_to_string(buffer_.data());
                // run user function
                ws_handler_(msg, *this);
            } catch (std::exception &e) {
                fail_log(ec, e.what());
            }

            // clear the request object
            req_ = {};

            // clear the buffers
            buffer_.consume(buffer_.size());

            this->do_read();
        }

        void on_write(beast::error_code ec, std::size_t bytes) {
            boost::ignore_unused(bytes);

            // happens when the timer closes the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }

            if (ec) {
                fail_log(ec, "write");
                return;
            }

            // remove sent message from the queue
            que_.pop_front();

            if (que_.empty()) {
                return;
            }

            ws_.async_write(
                    asio::buffer(*que_.front()),
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_write,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2)));
        }

        void run() {
            this->do_accept();
        }

        void do_timeout() {
            this->do_shutdown();
        }

        void do_shutdown() {
            ws_.async_close(websocket::close_code::normal,
                            asio::bind_executor(this->strand_,
                                                std::bind(
                                                        &WebsocketSession::on_shutdown,
                                                        shared_from_this(),
                                                        std::placeholders::_1)));
        }

        void on_shutdown(beast::error_code ec) {
            if (ec) {
                fail_log(ec, "shutdown");
                return;
            }
        }

    private:
        websocket::stream <tcp::socket> ws_;
        Attr &attr_;
        HttpRequest req_;
        WebsocketHandler const &ws_handler_;
        asio::strand <asio::io_context::executor_type> strand_;
        boost::beast::multi_buffer buffer_;
        std::deque<std::shared_ptr<std::string const>> que_{};
    }; // class Webws_Base

    typedef std::shared_ptr<WebsocketSession> WebsocketSessionPtr;
}