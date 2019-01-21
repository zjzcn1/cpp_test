#pragma once

#include "common.h"
#include "http_utils.h"
#include "concurrent_queue.h"

namespace http_server {
    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
    private:
        websocket::stream <tcp::socket> ws_;
        asio::strand <asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        beast::multi_buffer buffer_;
        char ping_state_ = 0;
        Attr &attr_;
        WebsocketHandler const &ws_handler_;
        HttpRequest req_;
        ConcurrentQueue<std::shared_ptr<std::string const>> queue_;

    public:
        // Take ownership of the socket
        explicit WebsocketSession(tcp::socket socket, Attr &attr, HttpRequest &&req, WebsocketHandler const &ws_handler)
                : ws_(std::move(socket)),
                  strand_(ws_.get_executor()),
                  timer_(ws_.get_executor().context(), (std::chrono::steady_clock::time_point::max) ()),
                  attr_(attr),
                  req_(std::move(req)),
                  ws_handler_(ws_handler),
                  queue_(1000){
        }

        ~WebsocketSession() {
            // leave channel
            std::lock_guard<std::mutex> locker(attr_.channels_mutex);
            attr_.websocket_channels[req_.path].remove(*this);
        }

        void send(std::string const &msg) {
            auto const pmsg = std::make_shared<std::string const>(std::move(msg));
            queue_.put(pmsg);

            if (queue_.size() > 1) {
                return;
            }
            ws_.async_write(asio::buffer(*queue_.front()),
                            std::bind(&WebsocketSession::on_write, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
        }

        void on_write( beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            // happens when the timer closes the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }

            if (ec) {
                Logger::error("WebsocketSession on_write error,  {}.", ec.message());
                return;
            }

            // remove sent message from the queue
            queue_.pop_front();

            if (queue_.empty()) {
                return;
            }

            ws_.async_write(asio::buffer(*queue_.front()),
                            std::bind(&WebsocketSession::on_write, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
        }

        // Start the asynchronous operation
        void do_accept() {
            // Set the control callback. This will be called
            // on every incoming ping, pong, and close frame.
            ws_.control_callback(
                    std::bind(
                            &WebsocketSession::on_control_callback,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2));

            // Run the timer. The timer is operated
            // continuously, this simplifies the code.
            on_timer({});

            // Set the timer
            timer_.expires_after(std::chrono::seconds(attr_.timeout));

            // Accept the websocket handshake
            ws_.async_accept(
                    req_,
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_accept,
                                    shared_from_this(),
                                    std::placeholders::_1)));
        }

        void on_accept(beast::error_code ec) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }

            if (ec) {
                Logger::error("WebsocketSession on_accept error,  {}.", ec.message());
                return;
            }

            // join channel
            std::lock_guard<std::mutex> locker(attr_.channels_mutex);
            if (attr_.websocket_channels.find(req_.path) == attr_.websocket_channels.end()) {
                attr_.websocket_channels[req_.path] = WebsocketChannel();
            }
            attr_.websocket_channels[req_.path].insert(*this);
            Logger::info("WebsocketSession session insert to channel[{}].", req_.path);
            // Read a message
            do_read();
        }

        // Called when the timer expires.
        void on_timer(beast::error_code ec) {
            if (ec && ec != asio::error::operation_aborted) {
                Logger::error("WebsocketSession on_timer error,  {}.", ec.message());
                return;
            }

            // See if the timer really expired since the deadline may have moved.
            if (timer_.expiry() <= std::chrono::steady_clock::now()) {
                // If this is the first time the timer expired,
                // send a ping to see if the other end is there.
                if (ws_.is_open() && ping_state_ == 0) {
                    // Note that we are sending a ping
                    ping_state_ = 1;

                    // Set the timer
                    timer_.expires_after(std::chrono::seconds(15));

                    // Now send the ping
                    ws_.async_ping({},
                                   asio::bind_executor(
                                           strand_,
                                           std::bind(
                                                   &WebsocketSession::on_ping,
                                                   shared_from_this(),
                                                   std::placeholders::_1)));
                } else {
                    // The timer expired while trying to handshake,
                    // or we sent a ping and it never completed or
                    // we never got back a control frame, so close.

                    // Closing the socket cancels all outstanding operations. They
                    // will complete with asio::error::operation_aborted
                    ws_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
                    ws_.next_layer().close(ec);
                    return;
                }
            }

            // Wait on the timer
            timer_.async_wait(
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_timer,
                                    shared_from_this(),
                                    std::placeholders::_1)));
        }

        // Called to indicate set_active from the remote peer
        void set_active() {
            // Note that the connection is alive
            ping_state_ = 0;

            // Set the timer
            timer_.expires_after(std::chrono::seconds(attr_.timeout));
        }

        // Called after a ping is sent.
        void on_ping(beast::error_code ec) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            if (ec) {
                Logger::error("WebsocketSession on_ping error,  {}.", ec.message());
                return;
            }

            // Note that the ping was sent.
            if (ping_state_ == 1) {
                ping_state_ = 2;
            } else {
                // ping_state_ could have been set to 0
                // if an incoming control frame was received
                // at exactly the same time we sent a ping.
                BOOST_ASSERT(ping_state_ == 0);
            }
        }

        void on_control_callback(websocket::frame_type kind, beast::string_view payload) {
            boost::ignore_unused(kind, payload);

            // Note that there is set_active
            set_active();
        }

        void do_read() {
            // Read a message into our buffer
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

        void on_read(beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted) {
                return;
            }

            // This indicates that the WebsocketSession was closed
            if (ec == websocket::error::closed) {
                return;
            }

            if (ec) {
                Logger::error("WebsocketSession on_read error,  {}.", ec.message());
                return;
            }

            // Note that there is set_active
            set_active();

            try {
                std::string msg = beast::buffers_to_string(buffer_.data());
                // run user function
                ws_handler_(msg, *this);
            } catch (std::exception &e) {
                Logger::error("WebsocketSession handle websocket error,  {}.", e.what());
            }

            // clear the buffers
            buffer_.consume(buffer_.size());

            this->do_read();
        }

    };

}