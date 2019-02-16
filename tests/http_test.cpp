#include "../http/http_server.h"
#include "./map_loader.h"
#include "../http/base64.h"
#include "../http/json.hpp"

#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/copy.hpp>

using json = nlohmann::json;
using namespace http_server;
using namespace walle;

int main(int argc, char *argv[]) {
    MapLoader map_loader("data/map.yaml");
    OccupancyGridConstPtr map = map_loader.getMap();
    Logger::info("width={}, height={}", map->info.width, map->info.height);
    HttpServer server("0.0.0.0", 8084);
    server.register_http_handler("/hehe", HttpMethod::get, [](HttpRequest &req, HttpResponse &res) {
        std::cout << "hehe" << std::endl;
        res.body() = "hehe";
    });

    server.register_ws_handler("/ws", [&](std::string msg, WebsocketSession &session) {
        std::cout << msg << std::endl;

        if (msg == "path") {
            json path = {{"op",   "path"},
                         {"data", {
                                          {{"x", -8}, {"y", -10}, {"z", 0}},
                                          {{"x", -5}, {"y", -3}, {"z", 0}},
                                          {{"x", 0}, {"y", 0}, {"z", 0}},
                                          {{"x", 5}, {"y", 5}, {"z", 0}}
                                  }}};
            std::string data = path.dump();
            Logger::info("path={}", data);
            session.send(data);
        } else if (msg == "map") {
            std::vector<char> packed;
            boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
            out.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression, 65536));
            out.push(boost::iostreams::back_inserter(packed));
            boost::iostreams::copy(boost::iostreams::basic_array_source<char>((char *) &map->data[0], map->data.size()),
                                   out);

            Logger::info("map size={}, compress size={}", map->data.size(), packed.size());
            std::string base64_res = Base64::encode((unsigned char *) &packed[0], packed.size());
            json j2 = {{"op",   "map"},
                       {"data", base64_res}};
            std::string data = j2.dump();
            session.send(data);
        } else if (msg == "pose") {

        } else if (msg == "path1") {

        }
    });

    server.register_ws_handler("/ws1", [&](std::string msg, WebsocketSession &session) {
        std::cout << msg << std::endl;
//        session.send(msg);
        server.broadcast("/ws1", msg + "-----");
    });

    for (int j = 0; j < 1; j++) {
        std::thread([&]() {
            int i = 0;
            while (true) {
                if (i > 100) {
                    i = 0;
                }
                json j2 = {{"op",   "pose"},
                           {"data", i++ * 0.01}};
                std::string data = j2.dump();
                server.broadcast("/ws", data);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }).detach();
    }


    if (server.start()) {
        server.sync();
    }

    return 0;
}