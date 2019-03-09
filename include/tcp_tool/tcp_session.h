#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <boost/asio.hpp>

#include "util/logger.h"

namespace tcp_tool {

    using namespace util;
    using tcp = boost::asio::ip::tcp;

    template<typename T>
    class TcpSession;

    using ErrorCallback = std::function<void(long)>;
    template<typename T>
    using TcpEncoder = std::function<void(T &, std::vector<uint8_t> &)>;
    template<typename T>
    using TcpDecoder = std::function<bool(std::vector<uint8_t> &, std::size_t, T &)>;
    template<typename T>
    using TcpHandler = std::function<void(T &, TcpSession<T> &)>;

    template<typename T>
    class TcpSession : public std::enable_shared_from_this<TcpSession<T>> {
    public:
        TcpSession(tcp::socket socket,
                   TcpEncoder<T> encoder,
                   TcpDecoder<T> decoder,
                   TcpHandler<T> handler,
                   ErrorCallback error_callback)
                : socket_(std::move(socket)), encoder_(encoder), decoder_(decoder),
                  handler_(handler), error_callback_(error_callback), read_buffer_(max_buffer_length),
                  session_id_(generate_id()) {
        }

        long session_id() {
            return session_id_;
        }

        void start() {
            do_read();
        }

        void send(T &msg) {
            std::shared_ptr<std::vector<uint8_t>> data(new std::vector<uint8_t>());
            encoder_(msg, *data);

            std::lock_guard<std::mutex> locker(mutex_);
            write_queue_.emplace_back(data);
            if (write_queue_.size() > 1) {
                return;
            }

            do_write();
        }

    private:
        static long generate_id() {
            static std::atomic_long id(1);
            return id++;
        }

        void do_read() {
            socket_.async_read_some(boost::asio::buffer(read_buffer_),
                                    [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                                        if (ec) {
                                            Logger::error("TcpSession",
                                                          "Read data error, session_id={}, error_message={}.",
                                                          session_id_, ec.message());
                                            error_callback_(session_id_);
                                            return;
                                        }

                                        // decode
                                        T msg;
                                        bool success = decoder_(read_buffer_, bytes_transferred, msg);
                                        if (success) {
                                            handler_(msg, *this);
                                        }
                                        do_read();
                                    });
        }

        void do_write() {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(*write_queue_.front()),
                                     [this](boost::system::error_code ec, std::size_t /*length*/) {
                                         if (ec) {
                                             Logger::error("TcpSession",
                                                           "Write data error, session_id={}, error_message={}.",
                                                           session_id_, ec.message());
                                             error_callback_(session_id_);
                                         }

                                         std::lock_guard<std::mutex> locker(mutex_);
                                         write_queue_.pop_front();
                                         if (write_queue_.empty()) {
                                             return;
                                         }

                                         do_write();
                                     });
        }

    private:
        enum {
            max_buffer_length = 4 * 1024 * 1024
        };
        long session_id_;
        tcp::socket socket_;
        std::vector<uint8_t> read_buffer_;
        std::mutex mutex_;
        std::deque<std::shared_ptr<std::vector<uint8_t>>> write_queue_;
        TcpEncoder<T> encoder_;
        TcpDecoder<T> decoder_;
        TcpHandler<T> handler_;
        ErrorCallback error_callback_;
    };

}