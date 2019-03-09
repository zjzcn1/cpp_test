#pragma once

#include "attr.h"
#include "http_utils.h"
#include "http_session.h"

namespace http_server {

    // Accepts incoming connections and launches the HttpSessions
    class Acceptor : public std::enable_shared_from_this<Acceptor> {
    public:
        Acceptor(asio::io_context &ioc, tcp::endpoint &&endpoint, Attr &attr)
                : endpoint_(std::move(endpoint)), acceptor_(ioc), attr_(attr) {
        }

        // Start accepting incoming connections
        void listen() {
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
                Logger::error("Acceptor", "On listen {}:{} error, {}.", endpoint_.address().to_string(),
                              endpoint_.port(), e.what());
                throw e;
            }

            do_accept();
        }

        void do_accept() {
            acceptor_.async_accept(
                    [this](boost::system::error_code ec, tcp::socket socket) {
                        if (ec) {
                            Logger::error("Acceptor", "On accept http session error, {}.", ec.message());
                        } else {
                            Logger::info("HttpSession", "New http session, host={}, port={}.",
                                         socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port());
                            HttpSessionPtr session(new HttpSession(std::move(socket), attr_));
                            std::lock_guard<std::mutex> locker(attr_.http_mutex);
                            attr_.http_sessions.insert(session);
                            session->run();

                        }

                        do_accept();
                    });
        }

    private:
        tcp::endpoint endpoint_;
        tcp::acceptor acceptor_;
        Attr &attr_;
    };
}