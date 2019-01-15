#pragma once

#include "common.h"
#include "http_utils.h"

namespace http_server {
    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
    private:
        websocket::stream <tcp::socket> ws_;
        asio::strand <
        asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        beast::multi_buffer buffer_;
        char ping_state_ = 0;

    public:
        // Take ownership of the socket
        explicit
        WebsocketSession(tcp::socket
                          socket)
                :
                ws_(std::move(socket)
                ),
                strand_(ws_
                                .

                                        get_executor()

                ),
                timer_(ws_
                               .

                                       get_executor()

                               .

                                       context(),
                       (std::chrono::steady_clock::time_point::max) ()

                ) {
        }

// Start the asynchronous operation
        template<class Body, class Allocator>
        void
        do_accept(http::request <Body, http::basic_fields<Allocator>> req) {
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
            timer_.expires_after(std::chrono::seconds(15));

            // Accept the websocket handshake
            ws_.async_accept(
                    req,
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_accept,
                                    shared_from_this(),
                                    std::placeholders::_1)));
        }

        void
        on_accept(beast::error_code ec) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            if (ec)
                return fail_log(ec, "accept");

            // Read a message
            do_read();
        }

// Called when the timer expires.
        void
        on_timer(beast::error_code ec) {
            if (ec && ec != asio::error::operation_aborted)
                return fail_log(ec, "timer");

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

// Called to indicate activity from the remote peer
        void
        activity() {
            // Note that the connection is alive
            ping_state_ = 0;

            // Set the timer
            timer_.expires_after(std::chrono::seconds(15));
        }

// Called after a ping is sent.
        void
        on_ping(beast::error_code ec) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            if (ec)
                return fail_log(ec, "ping");

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

        void
        on_control_callback(
                websocket::frame_type kind,
                beast::string_view payload) {
            boost::ignore_unused(kind, payload);

            // Note that there is activity
            activity();
        }

        void
        do_read() {
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

        void
        on_read(
                beast::error_code ec,
                std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            // This indicates that the WebsocketSession was closed
            if (ec == websocket::error::closed)
                return;

            if (ec)
                fail_log(ec, "read");

            // Note that there is activity
            activity();

            // Echo the message
            ws_.text(ws_.got_text());
            ws_.async_write(
                    buffer_.data(),
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &WebsocketSession::on_write,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2)));
        }

        void
        on_write(
                beast::error_code ec,
                std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            if (ec)
                return fail_log(ec, "write");

            // Clear the buffer
            buffer_.consume(buffer_.size());

            // Do another read
            do_read();
        }
    };

}