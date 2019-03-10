
#include "tcp_tool/tcp_server.h"
#include "tcp_tool/tcp_client.h"

using namespace tcp_tool;

struct TestMessage {
    std::string data;
};

int main(int argc, char *argv[]) {

    TcpServer<TestMessage> server;

    server.encoder([](TestMessage &t, std::vector<char> &data) {
        Logger::warn("TcpServer", " tcp encoder");
        data.push_back('c');
        data.push_back('b');
    });
    server.decoder([](std::vector<char> &data, TestMessage &t) -> bool {
        Logger::warn("TcpServer", "tcp decoder, {}", data.size());
        std::string res;
        res.insert(res.begin(), data.begin(), data.end());
        t.data = res;
        data.clear();
        return true;
    });
    server.handler([&](TestMessage &msg, TcpSession<TestMessage> &session) {
        Logger::info("main", "{}", msg.data);
    });
    server.listen(8085);

    std::shared_ptr<TcpClient<TestMessage>> client(new TcpClient<TestMessage>());
    client->encoder([](TestMessage &t, std::vector<char> &data) {
        Logger::info("TcpClient", "tcp encoder");
        data.push_back('3');
        data.push_back('2');
        data.push_back('1');
//        for (int i=0; i<1000000000; i++) {
//            data.push_back('1');
//        }
    });
    client->decoder([](std::vector<char> &data, TestMessage &t) -> bool {
        Logger::warn("TcpClient", "tcp decoder, {}", data.size());
        std::string res;
        res.insert(res.begin(), data.begin(), data.end());
        t.data = res;
        data.clear();
        return true;
    });
    client->handler([&](TestMessage &msg, TcpSession<TestMessage> &session) {
        Logger::info("main client", "{}", msg.data);
    });

    std::thread([client]() {
        client->connect("127.0.0.1", 8085);
        while (true) {
            TestMessage msg;
            client->send(msg);
            sleep(3);
        }
    }).detach();

    for (int i=0; i<1000000; i++) {
        TestMessage msg;
        server.broadcast(msg);
        sleep(3);
    }

    sleep(10000000);
    return 0;
}