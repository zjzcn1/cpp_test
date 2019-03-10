#pragma once

#include <thread>

#include "tcp_session.h"

namespace tcp_tool {

    template<typename T>
    class TcpClient : public std::enable_shared_from_this<TcpClient<T>> {
    public:
        TcpClient()
                : socket_(ioc_),
                  encoder_([](T &t, std::vector<char> &data) {
                      Logger::warn("TcpClient", "Using default tcp encoder");
                  }),
                  decoder_([](std::vector<char> &data, T &t) -> bool {
                      Logger::warn("TcpClient", "Using default tcp decoder");
                      return true;
                  }),
                  handler_([](T &t, TcpSession<T> &session) {
                      Logger::warn("TcpClient", "Using default tcp handler");
                  }) {
        }

        ~TcpClient() {
            close();
        }

        void encoder(TcpEncoder<T> encoder) {
            encoder_ = encoder;
            Logger::info("TcpClient", "Set tpc encoder.");
        }

        void decoder(TcpDecoder<T> decoder) {
            decoder_ = decoder;
            Logger::info("TcpClient", "Set tpc decoder.");
        }

        void handler(TcpHandler<T> handler) {
            handler_ = handler;
            Logger::info("TcpClient", "Set tpc handler.");
        }

        void connect(const std::string &host, unsigned short port, bool sync = false) {
            try {
                endpoint_ = tcp::endpoint(boost::asio::ip::make_address(host), port);
                socket_.connect(endpoint_);

                Logger::info("TcpClient", "Tcp client connect successful, "
                                          "local_host={}, local_port={}, "
                                          "remote_host={}, remote_port={}.",
                             socket_.local_endpoint().address().to_string(),
                             socket_.local_endpoint().port(),
                             socket_.remote_endpoint().address().to_string(),
                             socket_.remote_endpoint().port());

                session_ = std::shared_ptr<TcpSession<T>>(
                        new TcpSession<T>(std::move(socket_), encoder_, decoder_, handler_));
                session_->start();
            } catch (std::exception &e) {
                Logger::error("Acceptor", "On connect[{}:{}] error, {}.", endpoint_.address().to_string(),
                              endpoint_.port(),
                              e.what());
                throw e;
            }

            io_thread_ = std::make_shared<std::thread>([this]() { ioc_.run(); });
            if (sync) {
                io_thread_->join();
            } else {
                io_thread_->detach();
            }
        }

        void close() {
            boost::asio::post(ioc_, [this]() { socket_.close(); });
        }

        void send(T &msg) {
            session_->send(msg);
        }

    private:
        tcp::endpoint endpoint_;
        boost::asio::io_context ioc_;
        tcp::socket socket_;
        std::shared_ptr<std::thread> io_thread_;

        TcpEncoder<T> encoder_;
        TcpDecoder<T> decoder_;
        TcpHandler<T> handler_;
        std::shared_ptr<TcpSession<T>> session_;
    };

}
