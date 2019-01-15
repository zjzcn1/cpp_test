#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <regex>
#include <unordered_set>
#include "logger.h"

namespace http_server {
    namespace beast = boost::beast;
    namespace http = boost::beast::http;
    namespace asio = boost::asio;
    namespace websocket = boost::beast::websocket;

    using tcp = boost::asio::ip::tcp;

    using HttpMethod = boost::beast::http::verb;
    using HttpStatus = boost::beast::http::status;
    using HttpHeader = boost::beast::http::field;
    using HttpHeaders = boost::beast::http::fields;

//    using HttpRequest = http::request<http::string_body>;

    struct HttpRequest : public http::request<http::string_body> {
        std::string path;
        std::unordered_multimap<std::string, std::string> params;
    };

    using HttpResponse = http::response<http::string_body>;
    using HttpResponsePtr = std::shared_ptr<HttpResponse>;
    using FileResponse = http::response<http::file_body>;
    using FileResponsePtr = std::shared_ptr<FileResponse>;

    using HttpHandler = std::function<void(HttpRequest &, HttpResponse &)>;

    using HttpRoutes = std::unordered_map<std::string, std::unordered_map<int, HttpHandler>>;

    class WebsocketSession;
    using WebsocketHandler = std::function<void(std::string &, WebsocketSession&)>;
    using WebsocketRoutes = std::vector<std::pair<std::string, WebsocketHandler>>;

    class WebsocketChannel {
    public:

        WebsocketChannel() = default;

        void insert(WebsocketSession &session) {
            sessions.insert(&session);
        }

        void remove(WebsocketSession &session) {
            sessions.erase(&session);
        }

        std::size_t size() const {
            return sessions.size();
        }

    public:
        std::unordered_set<WebsocketSession *> sessions;
    };

    using WebsocketChannels = std::unordered_map<std::string, WebsocketChannel>;

    struct Attr {
        std::string web_dir{"."};

        std::string index_file{"index.html"};

        std::chrono::seconds timeout{10};

        // default http headers
        HttpHeaders http_headers{};

        HttpRoutes http_routes{};

        WebsocketRoutes websocket_routes{};

        WebsocketChannels websocket_channels{};
    };

    void fail_log(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }

}