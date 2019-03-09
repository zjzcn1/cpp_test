#include "tcp_tool/tcp_server.h"

using namespace tcp_tool;
int main() {
    TcpServer server(6699);

    server.decoder([]() {

    });

    server.start(true);

}