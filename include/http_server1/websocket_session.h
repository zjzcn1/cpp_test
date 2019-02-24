#pragma once

#include "common.h"
#include "http_utils.h"

namespace http_server {
    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
    public:
        WebsocketSession(tcp::socket socket, Attr &attr, HttpRequest &&req,
                                  WebsocketHandler const &ws_handler)
                : websocket_(std::move(socket)),
                  strand_(websocket_.get_executor()),
                  timer_(websocket_.get_executor().context(), (std::chrono::steady_clock::time_point::max) ()),
                  attr_(attr),
                  req_(std::move(req)),
                  ws_handler_(ws_handler) {
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
                                    Logger::error("WebsocketSession", "On accept error, error_message={}.",
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
                                    Logger::error("WebsocketSession", "On read data error, error_message={}.",
                                                  ec.message());
                                    return;
                                }

                                // Note that there is set_active
                                set_active();

                                try {
                                    std::string msg = beast::buffers_to_string(read_buffer_.data());
                                    // run user function
                                    ws_handler_(msg, *this);
                                } catch (std::exception &e) {
                                    Logger::error("WebsocketSession",
                                                  "On handle websocket biz error, error_message={}.",
                                                  e.what());
                                }

                                // clear the buffers
                                read_buffer_.consume(read_buffer_.size());

                                this->do_read();
                            }));
        }

        void send(const std::string &msg) {
            std::shared_ptr<std::string> data = std::make_shared<std::string>(std::move(msg));

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
                                           Logger::error("WebsocketSession", "On write data error, error_message={}.",
                                                         ec.message());
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
                                if (ec && ec != asio::error::operation_aborted) {
                                    Logger::error("WebsocketSession", "On timer error, error_message={}.",
                                                  ec.message());
                                    return;
                                }

                                // See if the timer really expired since the deadline may have moved.
                                if (timer_.expiry() <= std::chrono::steady_clock::now()) {
                                    // If this is the first time the timer expired,
                                    // send a ping to see if the other end is there.
                                    if (websocket_.is_open() && ping_state_) {
                                        // Note that we are sending a ping
                                        ping_state_ = false;
                                        // Set the timer
                                        timer_.expires_after(std::chrono::seconds(15));

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
                                                                              "On ping error, error_message={}.",
                                                                              ec.message());
                                                                return;
                                                            }
                                                        }));

                                    } else {
                                        // The timer expired while trying to handshake,
                                        // or we sent a ping and it never completed or
                                        // we never got back a control frame, so close.

                                        // Closing the socket cancels all outstanding operations. They
                                        // will complete with asio::error::operation_aborted
                                        websocket_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
                                        websocket_.next_layer().close(ec);
                                        return;
                                    }

                                    do_timer();
                                }
                            }));
        }

        // Called to indicate set_active from the remote peer
        void set_active() {
            // Note that the connection is alive
            ping_state_ = true;

            // Set the timer
            timer_.expires_after(std::chrono::seconds(attr_.timeout));
        }

    private:
        websocket::stream<tcp::socket> websocket_;
        asio::strand<asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        bool ping_state_ = true;
        Attr &attr_;
        WebsocketHandler const &ws_handler_;
        HttpRequest req_;

        beast::multi_buffer read_buffer_;
        std::mutex mutex_;
        std::deque<std::shared_ptr<std::string const>> write_queue_;
    };

}