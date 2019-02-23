#pragma once

#include "acceptor.h"
#include "tcp_session.h"

namespace tcp_tool {

    template<typename T>
    class TcpServer {
    public:
        explicit TcpServer(unsigned short port)
                : port_(port), threads_(2),
                  encoder_([](T &t, std::vector<uint8_t> &data) {
                      Logger::warn("TcpServer", "Using default tcp encoder");
                  }),
                  decoder_([](std::vector<uint8_t> &data, std::size_t bytes_transferred, T &t) -> bool {
                      Logger::warn("TcpServer", "Using default tcp decoder");
                      return true;
                  }),
                  handler_([](T &t, TcpSession<T> &session) {
                      Logger::warn("TcpServer", "Using default tcp handler");
                  }){
        }

        ~TcpServer() {
            ioc_.stop();
        }

        void threads(int threads) {
            threads_ = threads;
            Logger::info("TcpServer", "Set tpc threads={}.", threads);
        }

        void encoder(TcpEncoder<T> encoder) {
            encoder_ = encoder;
            Logger::info("TcpServer", "Set tpc encoder.");
        }

        void decoder(TcpDecoder<T> decoder) {
            decoder_ = decoder;
            Logger::info("TcpServer", "Set tpc decoder.");
        }

        void handler(TcpHandler<T> handler) {
            handler_ = handler;
            Logger::info("TcpServer", "Set tpc handler.");
        }

        void broadcast(T &msg) {
            acceptor_->broadcast(msg);
        }

        void start(bool sync = false) {
            acceptor_ = std::make_shared<Acceptor<T>>(ioc_, port_, encoder_, decoder_, handler_);
            // listen server
            acceptor_->listen();

            // Run the I/O service on the requested number of threads
            io_threads_.reserve(static_cast<unsigned long>(threads_));
            for (int i = 0; i < threads_; i++) {
                io_threads_.emplace_back([this] {
                    ioc_.run();
                });
            }
            Logger::info("TcpServer", "Tcp server started successful, port={}, sync={}.", port_, sync);
            if (sync) {
                for (std::thread &t : io_threads_) {
                    t.join();
                }
            } else {
                for (std::thread &t : io_threads_) {
                    t.detach();
                }
            }
        }

    private:
        std::shared_ptr<Acceptor<T>> acceptor_;
        unsigned short port_;
        int threads_;
        boost::asio::io_context ioc_;
        std::vector<std::thread> io_threads_;
        TcpEncoder<T> encoder_;
        TcpDecoder<T> decoder_;
        TcpHandler<T> handler_;
    };
}