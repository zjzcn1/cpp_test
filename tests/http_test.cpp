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

        std::vector<char> packed;
        boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
        out.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression, 65536));
        out.push(boost::iostreams::back_inserter(packed));
        boost::iostreams::copy(boost::iostreams::basic_array_source<char>((char*)&map->data[0], map->data.size()), out);

        Logger::info("map size={}, compress size={}", map->data.size(), packed.size());

//        char a[map->data.size()];
//        for (int i = 0; i < map->data.size(); i++) {
//            a[i] = map->data.at(i);
//        }

//        uint8_t *buffer = new uint8_t[map->data.size()];
//        if (!map->data.empty()) {
//            memcpy(buffer, &map->data[0], map->data.size() * sizeof(uint8_t));
//        }
//        std::vector<unsigned char> png;
//        unsigned int error = lodepng::compress(png, (unsigned char *)map->data.data(), map->data.size());
//
//        LZ4Encoder encoder(8192);
//        char *buffer = new char[map->data.size()];
//        if (!map->data.empty()) {
//            memcpy(buffer, &map->data[0], map->data.size() * sizeof(int8_t));
//        }
//        char *compressed_msg = NULL;
//        size_t compressed_size = 0;
//
//        encoder.open(&compressed_msg, &compressed_size);
//
//        encoder.encode(buffer, map->data.size());
//        encoder.close();

//        Logger::info("map size={},  png size={}", map->data.size(), compressed_size);
//
//        std::string base64_map = Base64::encode((unsigned char*)compressed_msg, compressed_size);


//        if (!map->data.empty()) {
//            memcpy(buffer, &map->data[0], map->data.size() * sizeof(uint8_t));
//        }

//        json j2 = {{"base64_map", base64_map}};
//
//        std::string data = j2.dump();

//        std::vector<unsigned char> png;
//        unsigned int error = lodepng::compress(png, (unsigned char *)data.data(), data.length());
//
//        Logger::info("error={}, ap size={},  png size={}", error, map->data.size(), png.size());

//        uint8_t *res = new uint8_t[png.size()];
//        if (!map->data.empty()) {
//            memcpy(res, &png[0], png.size() * sizeof(uint8_t));
//        }
        std::string base64_res = Base64::encode((unsigned char *)&packed[0], packed.size());
//        Logger::info("base64_res={}", base64_res);
        session.send(base64_res);
    });

    server.register_ws_handler("/ws1", [&](std::string msg, WebsocketSession &session) {
        std::cout << msg << std::endl;
//        session.send(msg);
        server.broadcast("/ws1", msg + "-----");
    });

    if (server.start()) {
        server.sync();
    }

    return 0;
}