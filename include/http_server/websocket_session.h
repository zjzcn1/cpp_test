#pragma once

#include "attr.h"
#include "http_utils.h"

namespace http_server {
    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
    public:
        WebsocketSession(tcp::socket socket, Attr &attr, HttpRequest &&req)
                : websocket_(std::move(socket)),
                  strand_(websocket_.get_executor()),
                  timer_(websocket_.get_executor().context(), (std::chrono::steady_clock::time_point::max) ()),
                  attr_(attr),
                  req_(std::move(req)) {
            websocket_.binary(true);
        }

        // Start the asynchronous operation
        void do_accept() {
            // Set the control callback. This will be called
            // on every incoming ping, pong, and close frame.
            websocket_.control_callback(
                    [this](websocket::frame_type kind, beast::string_view payload) {
                        boost::ignore_unused(kind, payload);
                        // Note that there is set_active
                        set_active();
                    });

            do_timer();

            // Accept the websocket handshake
            websocket_.async_accept(
                    req_,
                    asio::bind_executor(
                            strand_,
                            [this](boost::system::error_code ec) {
                                if (ec == asio::error::operation_aborted) {
                                    return;
                                }
                                if (ec) {
                                    Logger::error("WebsocketSession", "Websockt accept error, error_message={}.",
                                                  ec.message());
                                    return;
                                }

                                do_read();
                            }));

        }

        void do_read() {
            // Read a message into our buffer
            websocket_.async_read(
                    read_buffer_,
                    asio::bind_executor(
                            strand_,
                            [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                                // Happens when the timer closes the socket
                                if (ec == asio::error::operation_aborted) {
                                    return;
                                }
                                // This indicates that the WebsocketSession was closed
                                if (ec == websocket::error::closed) {
                                    return;
                                }
                                if (ec) {
                                    Logger::error("WebsocketSession", "Websocket read data error, error_message={}.",
                                                  ec.message());
                                    do_close(ec);
                                    return;
                                }

                                // Note that there is set_active
                                set_active();

                                try {
                                    std::shared_ptr<std::vector<char>> data = HttpUtils::buffers_to_vector(
                                            read_buffer_.data());
                                    attr_.websocket_handler(*data, *this);
                                } catch (std::exception &e) {
                                    Logger::error("WebsocketSession", "Call WebsocketHandler error, error_message={}.",
                                                  e.what());
                                }

                                // clear the buffers
                                read_buffer_.consume(read_buffer_.size());

                                this->do_read();
                            }));
        }

        void send(std::shared_ptr<std::vector<char>> data) {
            std::lock_guard<std::mutex> locker(mutex_);
            write_queue_.emplace_back(data);
            if (write_queue_.size() > 1) {
                return;
            }

            do_write();
        }

        void do_write() {
            websocket_.async_write(boost::asio::buffer(*write_queue_.front()),
                                   [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                                       boost::ignore_unused(bytes_transferred);
                                       // happens when the timer closes the socket
                                       if (ec == asio::error::operation_aborted) {
                                           return;
                                       }
                                       if (ec) {
                                           Logger::error("WebsocketSession",
                                                         "Websocket write data error, error_message={}.",
                                                         ec.message());
                                           do_close(ec);
                                           return;
                                       }

                                       std::lock_guard<std::mutex> locker(mutex_);
                                       write_queue_.pop_front();
                                       if (write_queue_.empty()) {
                                           return;
                                       }

                                       do_write();
                                   });
        }

        void do_timer() {
            timer_.async_wait(
                    asio::bind_executor(
                            strand_,
                            [this](boost::system::error_code ec) {
                                if (ec == asio::error::operation_aborted) {
                                    return;
                                }
                                if (ec) {
                                    Logger::error("WebsocketSession", "Timer wait error, error_message={}.",
                                                  ec.message());
                                    return;
                                }

                                // See if the timer really expired since the deadline may have moved.
                                if (timer_.expiry() <= std::chrono::steady_clock::now()) {
                                    // If this is the first time the timer expired,
                                    // send a ping to see if the other end is there.
                                    if (websocket_.is_open() && ping_state_) {
                                        ping_state_ = false;

                                        // Now send the ping
                                        websocket_.async_ping(
                                                {},
                                                asio::bind_executor(
                                                        strand_,
                                                        [this](beast::error_code ec) {
                                                            if (ec == asio::error::operation_aborted) {
                                                                return;
                                                            }
                                                            if (ec) {
                                                                Logger::error("WebsocketSession",
                                                                              "Websocket ping error, error_message={}.",
                                                                              ec.message());
                                                                return;
                                                            }
                                                        }));

                                    } else {
                                        do_close(ec);
                                        return;
                                    }
                                }
                            }));
        }

        // Called to indicate set_active from the remote peer
        void set_active() {
            // Note that the connection is alive
            ping_state_ = true;

            // Set the timer
            timer_.expires_after(attr_.timeout);
            do_timer();
        }

        void do_close(boost::system::error_code ec) {
            // The timer expired while trying to handshake,
            // or we sent a ping and it never completed or
            // we never got back a control frame, so close.

            // Closing the socket cancels all outstanding operations. They
            // will complete with asio::error::operation_aborted
            websocket_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
            websocket_.next_layer().close(ec);

            Logger::info("WebsocketSession", "Socket shutdown, and remove websocket session, session_id={}.",
                         session_id_);
            attr_.websocket_close_callback(*shared_from_this());
            std::lock_guard<std::mutex> locker(attr_.websocket_mutex);
            attr_.websocket_sessions.erase(shared_from_this());
        }

        long session_id() {
            return session_id_;
        }

        static long generate_id() {
            static std::atomic_long id(1);
            return id++;
        }

    private:
        long session_id_{generate_id()};
        websocket::stream <tcp::socket> websocket_;
        asio::strand <asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        bool ping_state_ = true;
        Attr &attr_;
        HttpRequest req_;

        beast::multi_buffer read_buffer_;
        std::mutex mutex_;
        std::deque<std::shared_ptr<std::vector<char>>> write_queue_;
    };

}