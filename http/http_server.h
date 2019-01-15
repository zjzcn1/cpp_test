#pragma once

#include "common.h"
#include "http_utils.h"
#include "acceptor.h"

namespace http_server {

    class HttpServer {
    public:
        HttpServer(std::string host, unsigned short port, int threads = 1)
                : host_(std::move(host)), port_(port), threads_(threads) {
        }

        HttpServer &web_dir(std::string web_dir) {
            if (!web_dir.empty() && web_dir.back() == '/') {
                web_dir.pop_back();
            }
            if (web_dir.empty()) {
                web_dir = ".";
            }
            attr_.web_dir = web_dir;
            return *this;
        }

        // get websocket upgrade
        HttpServer &support_websocket(bool support_websocket) {
            attr_.support_websocket = support_websocket;
            return *this;
        }

        // set the socket timeout
        HttpServer &timeout(std::chrono::seconds timeout) {
            attr_.timeout = timeout;
            return *this;
        }

        HttpServer &register_http_handler(std::string url_regx, Method method, HttpHandler handler) {
            if (attr_.http_routes.find(url_regx) == attr_.http_routes.end()) {
                attr_.http_routes[url_regx] = {{static_cast<int>(method), handler}};
            } else {
                attr_.http_routes.at(url_regx)[static_cast<int>(method)] = handler;
            }

            return *this;
        }

        HttpServer &register_http_handler(std::string url_regx, HttpHandler handler) {
            if (attr_.http_routes.find(url_regx) == attr_.http_routes.end()) {
                attr_.http_routes[url_regx] = {{static_cast<int>(Method::unknown), handler}};
            } else {
                attr_.http_routes.at(url_regx)[static_cast<int>(Method::unknown)] = handler;
            }

            return *this;
        }

        HttpServer &register_ws_handler(std::string url_regx, WebsocketHandler handler) {
            attr_.websocket_routes.emplace_back(std::make_pair(url_regx, handler));

            return *this;
        }

        HttpServer &start() {
            // Create and launch a listening port;
            std::make_shared<Acceptor>(
                    ioc_,
                    tcp::endpoint{asio::ip::make_address(host_), port_},
                    attr_)->run();

            // Run the I/O service on the requested number of threads
            io_threads_.reserve(threads_);
            for (int i = 0; i < threads_; i++) {
                io_threads_.emplace_back([this] {
                    ioc_.run();
                });
            }
            return *this;
        }

        void sync() {
            for (std::thread &t : io_threads_) {
                t.join();
            }
        }

    private:
        Attr attr_;

        std::string host_;
        unsigned short port_;
        int threads_;
        asio::io_context ioc_;
        std::vector<std::thread> io_threads_;

    };
}