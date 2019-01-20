#include <utility>

#pragma once

#include "common.h"
#include "http_utils.h"
#include "http_session.h"

namespace http_server {

// Accepts incoming connections and launches the HttpSessions
    class Acceptor : public std::enable_shared_from_this<Acceptor> {
    private:
        tcp::endpoint endpoint_;
        tcp::acceptor acceptor_;
        tcp::socket socket_;
        Attr &attr_;

    public:
        Acceptor(asio::io_context &ioc, tcp::endpoint &&endpoint, Attr &attr)
                : endpoint_(std::move(endpoint)), acceptor_(ioc), socket_(ioc), attr_(attr) {
        }

        // Start accepting incoming connections
        bool listen() {
            try {
                // Open the acceptor
                acceptor_.open(endpoint_.protocol());

                // Allow address reuse
                acceptor_.set_option(asio::socket_base::reuse_address(true));

                // Bind to the server address
                acceptor_.bind(endpoint_);

                // Start listening for connections
                acceptor_.listen(asio::socket_base::max_listen_connections);
            } catch (std::exception &e) {
                Logger::error("Acceptor listen[{}:{}] error: {}.", endpoint_.address().to_string(), endpoint_.port(), e.what());
                return false;
            }

            do_accept();
            return true;
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