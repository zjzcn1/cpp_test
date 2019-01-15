#include "http/http_server.h"

using namespace http_server;

int main(int argc, char *argv[]) {
    // Check command line arguments.
//    if (argc != 5) {
//        std::cerr <<
//                  "Usage: http-server-async <address> <port> <doc_root> <threads>\n" <<
//                  "Example:\n" <<
//                  "    http-server-async 0.0.0.0 8080 . 1\n";
//        return EXIT_FAILURE;
//    }
//    auto const address = asio::ip::make_address(argv[1]);
//    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
//    auto const doc_root = std::string(argv[3]);
//    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    HttpServer *server = new HttpServer("0.0.0.0", 8084);
    server->register_http_handler("/hehe", HttpMethod::get, [](HttpRequest &req, HttpResponse &res) {
        std::cout << "hehe" << std::endl;
        res.body() = "hehe";
    });

    server->register_ws_handler("/ws", [&](std::string msg, WebsocketSession &session) {
        std::cout << msg << std::endl;
//        session.send(msg);
        server->broadcast("/ws", msg + "-----");
    });

    server->register_ws_handler("/ws1", [&](std::string msg, WebsocketSession &session) {
        std::cout << msg << std::endl;
//        session.send(msg);
        server->broadcast("/ws1", msg + "-----");
    });

    server->start().sync();
////     The io_context is required for all I/O
//    asio::io_context ioc{threads};
//
//    // Create and launch a listening port
//    std::make_shared<Acceptor>(
//            ioc,
//            tcp::endpoint{address, port},
//            doc_root)->accept();
//
//    // Run the I/O service on the requested number of threads
//    std::vector<std::thread> v;
////    v.reserve(threads);
//    for (auto i = threads; i > 0; --i) {
//        v.emplace_back([&ioc] {
//            ioc.run();
//        });
//    }

//    std::mutex mutex_;
//    std::condition_variable cond_;
//    std::unique_lock<std::mutex> locker(mutex_);
//    cond_.wait(locker);

    return EXIT_SUCCESS;
}