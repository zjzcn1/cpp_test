#pragma once

#include <map>
#include "tcp_session.h"

namespace tcp_tool {

    template<typename T>
    class Acceptor : public std::enable_shared_from_this<Acceptor<T>> {
    public:
        Acceptor(boost::asio::io_context &ioc, unsigned short port,
                 TcpEncoder<T> encoder,
                 TcpDecoder<T> decoder,
                 TcpHandler<T> handler)
                : endpoint_(tcp::v4(), port), acceptor_(ioc),
                  encoder_(encoder), decoder_(decoder), handler_(handler) {
        }

        // Start accepting incoming connections
        void listen() {
            try {
                // Open the acceptor
                acceptor_.open(endpoint_.protocol());

                // Allow address reuse
                acceptor_.set_option(boost::asio::socket_base::reuse_address(true));

                // Bind to the server address
                acceptor_.bind(endpoint_);

                // Start listening for connections
                acceptor_.listen(boost::asio::socket_base::max_listen_connections);
            } catch (std::exception &e) {
                Logger::error("Acceptor", "On listen[{}:{}] error, {}.", endpoint_.address().to_string(),
                              endpoint_.port(),
                              e.what());
                throw e;
            }

            do_accept();
        }

        void broadcast(T &msg) {
            std::lock_guard<std::mutex> locker(mutex_);
            for (std::pair<long, std::shared_ptr<TcpSession<T>>> pair : sessions_) {
                pair.second->send(msg);
            }
        }
    private:
        void do_accept() {
            acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    Logger::info("Acceptor", "Accept tcp new connection, remote_host={}, remote_port={}.",
                                 socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port());

                    std::shared_ptr<TcpSession<T>> session(
                            new TcpSession<T>(std::move(socket), encoder_, decoder_, handler_,
                                              [this](long session_id) {
                                                  Logger::info("Acceptor", "Remove tcp session, session_id={}.",
                                                               session_id);
                                                  std::lock_guard<std::mutex> locker(mutex_);
                                                  sessions_.erase(session_id);
                                              }));
                    std::lock_guard<std::mutex> locker(mutex_);
                    sessions_[session->session_id()] = session;
                    session->start();
                    Logger::info("Acceptor", "Add new tcp session, session_id={}.",
                                 session->session_id());
                } else {
                    Logger::error("Acceptor", "Accept error, error_message={}.", ec.message());
                }

                do_accept();
            });
        }

    private:
        tcp::endpoint endpoint_;
        tcp::acceptor acceptor_;

        TcpEncoder<T> encoder_;
        TcpDecoder<T> decoder_;
        TcpHandler<T> handler_;
        std::mutex mutex_;
        std::map<long, std::shared_ptr<TcpSession<T>>> sessions_;
    };
}