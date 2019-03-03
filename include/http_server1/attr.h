#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <regex>
#include <set>
#include <map>
#include <mutex>

#include <boost/beast.hpp>
#include <boost/asio.hpp>

#include "util/logger.h"

namespace http_server {

    using Logger = util::Logger;

    namespace beast = boost::beast;
    namespace http = boost::beast::http;
    namespace asio = boost::asio;
    namespace websocket = boost::beast::websocket;

    using tcp = boost::asio::ip::tcp;

    using HttpMethod = boost::beast::http::verb;
    using HttpStatus = boost::beast::http::status;
    using HttpHeader = boost::beast::http::field;
    using HttpHeaders = boost::beast::http::fields;

    struct HttpRequest : public http::request<http::string_body> {
        std::string path;
        std::unordered_multimap<std::string, std::string> params;
    };

    using HttpResponse = http::response<http::string_body>;
    using HttpResponsePtr = std::shared_ptr<HttpResponse>;
    using FileResponse = http::response<http::file_body>;
    using FileResponsePtr = std::shared_ptr<FileResponse>;

    using HttpHandler = std::function<void(HttpRequest &, HttpResponse &)>;

    class HttpSession;

    using HttpSessionPtr = std::shared_ptr<HttpSession>;

    class WebsocketSession;

    using WebsocketSessionPtr = std::shared_ptr<WebsocketSession>;

    using WebsocketHandler = std::function<void(std::vector<char> &, WebsocketSession &)>;

    using WebsocketCloseCallback = std::function<void(WebsocketSession &)>;

    struct Attr {
        std::string webroot{"."};

        std::string index_file{"index.html"};

        std::chrono::seconds timeout{10};

        // default http headers
        HttpHeaders http_headers{};

        std::map<std::string, std::map<int, HttpHandler>> http_routes;

        std::mutex http_mutex;
        std::set<HttpSessionPtr> http_sessions;

        std::mutex websocket_mutex;
        std::set<WebsocketSessionPtr> websocket_sessions;

        WebsocketHandler websocket_handler{[](std::vector<char> &, WebsocketSession &){}};
        WebsocketCloseCallback websocket_close_callback{[](WebsocketSession &) {}};
    };

}