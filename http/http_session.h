#pragma once

#include "common.h"
#include "http_utils.h"
#include "websocket_session.h"

namespace http_server {

// Handles an HTTP server connection
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
        // This queue is used for HTTP pipelining.
        class queue {
            enum {
                // Maximum number of responses we will queue
                        limit = 8
            };

            // The type-erased, saved work item
            struct work {
                virtual ~work() = default;

                virtual void operator()() = 0;
            };

            HttpSession &self_;
            std::vector<std::unique_ptr<work>> items_;

        public:
            explicit
            queue(HttpSession &self)
                    : self_(self) {
                static_assert(limit > 0, "queue limit must be positive");
                items_.reserve(limit);
            }

            // Returns `true` if we have reached the queue limit
            bool
            is_full() const {
                return items_.size() >= limit;
            }

            // Called when a message finishes sending
            // Returns `true` if the caller should initiate a read
            bool
            on_write() {
                BOOST_ASSERT(!items_.empty());
                auto const was_full = is_full();
                items_.erase(items_.begin());
                if (!items_.empty())
                    (*items_.front())();
                return was_full;
            }

            // Called by the HTTP handler to send a response.
            template<bool isRequest, class Body, class Fields>
            void
            operator()(http::message<isRequest, Body, Fields> &&msg) {
                // This holds a work item
                struct work_impl : work {
                    HttpSession &self_;
                    http::message<isRequest, Body, Fields> msg_;

                    work_impl(
                            HttpSession &self,
                            http::message<isRequest, Body, Fields> &&msg)
                            : self_(self), msg_(std::move(msg)) {
                    }

                    void
                    operator()() {
                        http::async_write(
                                self_.socket_,
                                msg_,
                                asio::bind_executor(
                                        self_.strand_,
                                        std::bind(
                                                &HttpSession::on_write,
                                                self_.shared_from_this(),
                                                std::placeholders::_1,
                                                msg_.need_eof())));
                    }
                };

                // Allocate and store the work
                items_.push_back(
                        boost::make_unique<work_impl>(self_, std::move(msg)));

                // If there was no previous work, start this one
                if (items_.size() == 1)
                    (*items_.front())();
            }
        };

        tcp::socket socket_;
        asio::strand <
        asio::io_context::executor_type> strand_;
        asio::steady_timer timer_;
        beast::flat_buffer buffer_;
        Attr &attr_;
        http::request<http::string_body> req_;
        queue queue_;

    public:
        // Take ownership of the socket
        explicit
        HttpSession(
                tcp::socket socket,
                Attr &attr)
                : socket_(std::move(socket)), strand_(socket_.get_executor()), timer_(socket_.get_executor().context(),
                                                                                      (std::chrono::steady_clock::time_point::max) ()),
                  attr_(attr), queue_(*this) {
        }

        // Start the asynchronous operation
        void
        run() {
            // Make sure we run on the strand
            if (!strand_.running_in_this_thread())
                return asio::post(
                        asio::bind_executor(
                                strand_,
                                std::bind(
                                        &HttpSession::run,
                                        shared_from_this())));

            // Run the timer. The timer is operated
            // continuously, this simplifies the code.
            on_timer({});

            do_read();
        }

        void
        do_read() {
            // Set the timer
            timer_.expires_after(std::chrono::seconds(15));

            // Make the request empty before reading,
            // otherwise the operation behavior is undefined.
            req_ = {};

            // Read a request
            http::async_read(socket_, buffer_, req_,
                             asio::bind_executor(
                                     strand_,
                                     std::bind(
                                             &HttpSession::on_read,
                                             shared_from_this(),
                                             std::placeholders::_1)));
        }

        // Called when the timer expires.
        void
        on_timer(beast::error_code ec) {
            if (ec && ec != asio::error::operation_aborted)
                return fail_log(ec, "timer");

            // Check if this has been upgraded to Websocket
            if (timer_.expiry() == (std::chrono::steady_clock::time_point::min) ())
                return;

            // Verify that the timer really expired since the deadline may have moved.
            if (timer_.expiry() <= std::chrono::steady_clock::now()) {
                // Closing the socket cancels all outstanding operations. They
                // will complete with asio::error::operation_aborted
                socket_.shutdown(tcp::socket::shutdown_both, ec);
                socket_.close(ec);
                return;
            }

            // Wait on the timer
            timer_.async_wait(
                    asio::bind_executor(
                            strand_,
                            std::bind(
                                    &HttpSession::on_timer,
                                    shared_from_this(),
                                    std::placeholders::_1)));
        }

        void
        on_read(beast::error_code ec) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            // This means they closed the connection
            if (ec == http::error::end_of_stream)
                return do_close();

            if (ec)
                return fail_log(ec, "read");

            // See if it is a WebSocket Upgrade
            if (websocket::is_upgrade(req_)) {
                // Make timer expire immediately, by setting expiry to time_point::min we can detect
                // the upgrade to websocket in the timer handler
                timer_.expires_at((std::chrono::steady_clock::time_point::min) ());

                // Create a WebSocket websocket_session by transferring the socket
                std::make_shared<WebsocketSession>(
                        std::move(socket_))->do_accept(std::move(req_));
                return;
            }

            // Send the response
//            handle_request(attr_.web_dir, std::move(req_), queue_);

            // If we aren't at the queue limit, try to pipeline another request
            if (!queue_.is_full())
                do_read();
        }

        void
        on_write(beast::error_code ec, bool close) {
            // Happens when the timer closes the socket
            if (ec == asio::error::operation_aborted)
                return;

            if (ec)
                return fail_log(ec, "write");

            if (close) {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                return do_close();
            }

            // Inform the queue that a write completed
            if (queue_.on_write()) {
                // Read another request
                do_read();
            }
        }

        void
        do_close() {
            // Send a TCP shutdown
            beast::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_send, ec);

            // At this point the connection is closed gracefully
        }
    };
}