#pragma once

#include "common.h"
#include "http_utils.h"
#include "acceptor.h"

namespace http_server {

    class HttpServer {
    public:
        HttpServer(std::string host, int port, int threads)
                : host_(std::move(host)), port_(port), threads_(threads) {
        }

        explicit HttpServer(int port)
                : host_("0.0.0.0"), port_(port), threads_(1) {
        }

        void webroot(std::string webroot) {
            if (!webroot.empty() && webroot.back() == '/') {
                webroot.pop_back();
            }
            if (webroot.empty()) {
                webroot = ".";
            }
            attr_.webroot = webroot;
        }

        // set the socket timeout
        void timeout(std::chrono::seconds timeout) {
            attr_.timeout = timeout;
        }

        void http(std::string url_regx, HttpMethod method, HttpHandler handler) {
            if (attr_.http_routes.find(url_regx) == attr_.http_routes.end()) {
                attr_.http_routes[url_regx] = {{static_cast<int>(method), handler}};
            } else {
                attr_.http_routes.at(url_regx)[static_cast<int>(method)] = handler;
            }
            Logger::info("HttpServer", "Register http handler, http_method={}, url={}",
                         http::to_string(method).to_string(), url_regx);
        }

        void http(std::string url_regx, HttpHandler handler) {
            if (attr_.http_routes.find(url_regx) == attr_.http_routes.end()) {
                attr_.http_routes[url_regx] = {{static_cast<int>(HttpMethod::unknown), handler}};
            } else {
                attr_.http_routes.at(url_regx)[static_cast<int>(HttpMethod::unknown)] = handler;
            }
            Logger::info("HttpServer", "Register http handler, http_method=ALL, url={}", url_regx);
        }

        void websocket(std::string url_regx, WebsocketHandler handler) {
            attr_.websocket_routes.insert(std::make_pair(url_regx, handler));
            Logger::info("HttpServer", "Register websocket handler, url={}", url_regx);
        }

        void start(bool sync = false) {
            acceptor_ = std::make_shared<Acceptor>(
                    ioc_,
                    tcp::endpoint{asio::ip::make_address(host_), port_},
                    attr_);
            acceptor_->listen();

            // Run the I/O service on the requested number of threads
            io_threads_.reserve(threads_);
            for (int i = 0; i < threads_; i++) {
                io_threads_.emplace_back([this] {
                    ioc_.run();
                });
            }
            Logger::info("HttpServer", "Http server started successful, host={}, port={}.", host_, port_);

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

        void broadcast(std::string path, std::string msg) {
            std::lock_guard<std::mutex> locker(attr_.websocket_mutex);
            if (attr_.websocket_sessions.find(path) == attr_.websocket_sessions.end()) {
                return;
            }
            for (auto const session : attr_.websocket_sessions[path]) {
                session->send(msg);
            }
        }

    private:
        std::shared_ptr<Acceptor> acceptor_;
        std::string host_;
        unsigned short port_;
        int threads_;
        asio::io_context ioc_;
        std::vector<std::thread> io_threads_;
        Attr attr_;

    };
}