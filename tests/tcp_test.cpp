
#include "tcp_tool/tcp_server.h"
#include "tcp_tool/tcp_client.h"

using namespace tcp_tool;

struct TestMessage {
    std::string data;
};

int main(int argc, char *argv[]) {

    TcpServer<TestMessage> server(8085);

    server.encoder([](TestMessage &t, std::vector<uint8_t> &data) {
        Logger::warn("TcpServer", " tcp encoder");
        data.push_back('c');
        data.push_back('b');
    });
    server.decoder([](std::vector<uint8_t> &data, std::size_t bytes_transferred, TestMessage &t) -> bool {
        Logger::warn("TcpServer", "tcp decoder, {} {}", data.size(), bytes_transferred);
        data.clear();
        return true;
    });
    server.handler([&](TestMessage &msg, TcpSession<TestMessage> &session) {
        Logger::info("main", "{}", msg.data);
    });

    server.start();

    std::list<std::shared_ptr<TcpClient<TestMessage>>> list;
    for (int i = 0; i < 2; i++) {
        std::shared_ptr<TcpClient<TestMessage>> client(new TcpClient<TestMessage>("127.0.0.1", 8085));
        client->encoder([](TestMessage &t, std::vector<uint8_t> &data) {
            Logger::info("TcpClient", "tcp encoder");
            data.push_back('3');
            data.push_back('2');
        });
        list.push_back(client);
        client->connect();
    }

    std::thread([list]() {
        int i=0;
        while (i++<2) {
            sleep(3);
            for (const auto &client : list) {
                TestMessage msg;
                client->send(msg);
            }
        }

        for (const auto &client : list) {
            client->close();
        }

    }).detach();

    sleep(10);

    for (int i=0; i<10; i++) {
        TestMessage msg;
        server.broadcast(msg);
    }

    sleep(10000000);
    return 0;
}