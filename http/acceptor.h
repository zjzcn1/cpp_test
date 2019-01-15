#pragma once

#include "common.h"
#include "http_utils.h"
#include "http_session.h"

namespace http_server {

// Accepts incoming connections and launches the HttpSessions
    class Acceptor : public std::enable_shared_from_this<Acceptor> {
    private:
        tcp::acceptor acceptor_;
        tcp::socket socket_;
        Attr &attr_;

    public:
        Acceptor(asio::io_context &ioc, tcp::endpoint endpoint, Attr &attr)
                : acceptor_(ioc), socket_(ioc), attr_(attr) {
            beast::error_code ec;
            // Open the acceptor
            acceptor_.open(endpoint.protocol(), ec);
            if (ec) {
                Logger::error("Acceptor open error, error_message: {}.", ec.message());
                return;
            }

            // Allow address reuse
            acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
            if (ec) {
                Logger::error("Acceptor set_option error, error_message: {}.", ec.message());
                return;
            }

            // Bind to the server address
            acceptor_.bind(endpoint, ec);
            if (ec) {
                Logger::error("Acceptor bind error, error_message: {}.", ec.message());
                return;
            }

            // Start listening for connections
            acceptor_.listen(
                    asio::socket_base::max_listen_connections, ec);
            if (ec) {
                Logger::error("Acceptor listen error, error_message: {}.", ec.message());
                return;
            }
        }

        // Start accepting incoming connections
        void run() {
            if (!acceptor_.is_open()) {
                Logger::error("Acceptor not opened.");
                return;
            }
            do_accept();
        }

        void do_accept() {
            acceptor_.async_accept(
                    socket_,
                    std::bind(
                            &Acceptor::on_accept,
                            shared_from_this(),
                            std::placeholders::_1));
        }

        void on_accept(beast::error_code ec) {
            if (ec) {
                Logger::error("Acceptor on_accept error, error_message: {}.", ec.message());
            } else {
                // Create the http_session and run it
                std::make_shared<HttpSession>(
                        std::move(socket_),
                        attr_)->run();
            }

            // Accept another connection
            do_accept();
        }
    };
}