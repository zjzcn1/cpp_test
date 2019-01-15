#pragma once

#include "common.h"
#include "http_utils.h"
#include "acceptor.h"

namespace http_server {

    class HttpServer {
    public:
        HttpServer(std::string host = "0.0.0.0", unsigned short port = 8080, int threads = 1)
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

        // set the socket timeout
        HttpServer &timeout(std::chrono::seconds timeout) {
            attr_.timeout = timeout;
            return *this;
        }

        HttpServer &register_http_handler(std::string url_regx, HttpMethod method, HttpHandler handler) {
            if (attr_.http_routes.find(url_regx) == attr_.http_routes.end()) {
                attr_.http_routes[url_regx] = {{static_cast<int>(method), handler}};
            } else {
                attr_.http_routes.at(url_regx)[static_cast<int>(method)] = handler;
            }
            Logger::info("HttpServer register http handler, http_method={}, url={}", static_cast<int>(method), url_regx);
            return *this;
        }

        HttpServer &register_http_handler(std::string url_regx, HttpHandler handler) {
            if (attr_.http_routes.find(url_regx) == attr_.http_routes.end()) {
                attr_.http_routes[url_regx] = {{static_cast<int>(HttpMethod::unknown), handler}};
            } else {
                attr_.http_routes.at(url_regx)[static_cast<int>(HttpMethod::unknown)] = handler;
            }
            Logger::info("HttpServer register http handler, http_method=unknown, url={}", url_regx);
            return *this;
        }

        HttpServer &register_ws_handler(std::string url_regx, WebsocketHandler handler) {
            attr_.websocket_routes.emplace_back(std::make_pair(url_regx, handler));
            Logger::info("HttpServer register websocket handler, url={}", url_regx);
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
            Logger::info("HttpServer started, host=\"{}\", port={}.", host_, port_);
            return *this;
        }

        void sync() {
            for (std::thread &t : io_threads_) {
                t.join();
            }
        }

        void broadcast(std::string path, std::string msg) {
            for (auto const e : attr_.websocket_channels.at(path).sessions) {
                e->send(msg);
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