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

namespace http_server {
    namespace beast = boost::beast;
    namespace http = boost::beast::http;
    namespace asio = boost::asio;
    namespace websocket = boost::beast::websocket;

    using tcp = boost::asio::ip::tcp;

    using Method = boost::beast::http::verb;
    using Status = boost::beast::http::status;
    using Header = boost::beast::http::field;
    using Headers = boost::beast::http::fields;

    using HttpRequest = http::request<http::string_body>;
    using HttpResponse = http::response<http::string_body>;
    using HttpResponsePtr = std::shared_ptr<HttpResponse>;
    using FileResponse = http::response<http::file_body>;
    using FileResponsePtr = std::shared_ptr<FileResponse>;

    using HttpHandler = std::function<void(HttpRequest &, HttpResponse &)>;

    using HttpRoutes = std::unordered_map<std::string, std::unordered_map<int, HttpHandler>>;

    class WebsocketSession;
    using WebsocketHandler = std::function<void(std::string &, WebsocketSession&)>;
    using WebsocketRoutes = std::vector<std::pair<std::string, WebsocketHandler>>;


    struct Attr {
        std::string web_dir{"."};

        std::string index_file{"index.html"};

        std::chrono::seconds timeout{10};

        bool support_websocket{true};

        // default http headers
        Headers http_headers{};

        HttpRoutes http_routes{};

        WebsocketRoutes websocket_routes{};

    };

    void fail_log(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }

}