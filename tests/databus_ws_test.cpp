#include <iostream>
#include <chrono>
#include "data_bus/data_bus.h"
#include "http_server/http_server.h"
#include "Pose.pb.h"
#include "Protocol.pb.h"
#include <json.hpp>

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/copy.hpp>

using namespace data_bus;
using namespace http_server;
using json = nlohmann::json;


/**
 *
 * import * as ProtoBuf from "protobufjs";

let protoDefine = `
  syntax = "proto3";
  package protocol;

  message Message
  {
    enum Type {
        PUB = 0;
        SUB = 1;
        UNSUB = 2;
    }
    bool gzip = 1;
    Type type = 2;
    string topic = 3;
    bytes data = 4;
  }
  message SubInfo
  {
    bool gzip = 1;
  }

  message UnSubInfo
  {
    int64 sub_id = 1;
  }

  message Pose
  {
    int32 id = 1;
    string name = 2;
  }
  `;

let builder = ProtoBuf.parse(protoDefine);

let Pose  = builder.root.lookupType("protocol.Pose");
let Message  = builder.root.lookupType("protocol.Message");
let SubInfo  = builder.root.lookupType("protocol.SubInfo");

export {Pose, Message, SubInfo};


 * / const ws = new ReconnectingWebSocket('ws://127.0.0.1:8084/ws');
ws.addEventListener('open', () => {
  console.log('ws opened.')
  Bus.$emit('ws_connected', true);

  ws.binaryType = 'arraybuffer';

  let sub = SubInfo.create({gzip: true});
  console.log(sub);
  let data = Pako.gzip(SubInfo.encode(sub).finish());
  console.log(data);
  const msg = Message.create({gzip: true, topic: 'chat', type: Message.Type.SUB, data: data});

  ws.send(Message.encode(msg).finish())
});

ws.addEventListener('close', () => {
  console.log('ws closed.')
  Bus.$emit('ws_connected', false);
});

ws.addEventListener('message', (event) => {
  console.log(event);
  let message = Message.decode(new Uint8Array(event.data));
  console.log(message)
  if (message.gzip) {
    let pose = Pako.inflate(message.data);
    let p = Pose.decode(new Uint8Array(pose));
    console.log(p)
  } else {
    let p = Pose.decode(new Uint8Array(message.data));
    console.log(p)
  }

});

 */
struct ChatMsg {
    explicit ChatMsg() {
        time = std::chrono::system_clock::now().time_since_epoch().count();
    }

    long time;

    typedef std::shared_ptr<ChatMsg const> ConstPtr;
};

struct DataBusTest {
    void onEvent(ConstPtr<ProtoMessage> e) {
//        std::cout << "onEvent: " << e->GetTypeName() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    bool is_stop = false;

    void start() {
        std::thread([this]() {
            while (!is_stop) {
                auto t = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                auto stats = DataBus::getTopicStats();
                for (const auto &s : stats) {
//                    Logger::info("TopicStats", "topic={}, publish_count={}.", s->topic, s->publish_count);
//                    for (auto stat : s->callback_stats) {
//                        Logger::info("QueueStats", "{}", stat->toString());
//                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }).detach();

        DataBus::subscribe<ProtoMessage>("chat", "main_test",
                                         std::bind(&DataBusTest::onEvent, this, std::placeholders::_1));

        std::thread([]() {
            while (1) {
                Ptr<msg::Pose> chat(new msg::Pose());
                chat->set_id(1);
                chat->set_name("hehe pose");
                DataBus::publish<msg::Pose>("chat", chat);

                Ptr<msg::Pose> chat1(new msg::Pose());
                chat1->set_id(1);
                chat1->set_name("hehe pose1");
                DataBus::publish<msg::Pose>("chat1", chat1);

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }).detach();

        HttpServer server(8888);
        server.on_http("/test", HttpMethod::get, [](HttpRequest &req, HttpResponse &resp) {
            std::cout << "test" << std::endl;
            resp.body() = "test";
        });

        std::map<long, long> map;
        server.on_websocket([this, &map](std::vector<char> &data, WebsocketSession &session) {
            protocol::Message message;
            message.ParseFromArray(data.data(), data.size());
            Logger::info("Websocket", "Received message, gzip={}, type={}, topic={}, size={}, session_id={}.",
                         message.gzip(), message.type(), message.topic(), data.size(), session.session_id());
            std::string topic = message.topic();
            const std::string &msg_data = message.data();

            std::vector<char> packed;
            if (message.gzip()) {
                boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
                out.push(boost::iostreams::gzip_decompressor());
                out.push(boost::iostreams::back_inserter(packed));
                boost::iostreams::copy(boost::iostreams::basic_array_source<char>(msg_data.data(), msg_data.size()),
                                       out);
            } else {
                packed.resize(msg_data.size());
                packed.assign(msg_data.begin(), msg_data.end());
            }
            if (message.type() == protocol::Message_Type::Message_Type_SUB) {
                protocol::SubInfo sub_info;
                sub_info.ParseFromArray(packed.data(), packed.size());

                bool is_gzip = sub_info.gzip();
                long sub_id = DataBus::subscribe<ProtoMessage>(
                        topic, "websocket",
                        [this, &session, topic, is_gzip](ConstPtr<ProtoMessage> msg) {
                            int size = msg->ByteSize();
                            std::vector<char> buf(size);
                            msg->SerializeToArray(buf.data(), size);
                            protocol::Message send_msg;
                            send_msg.set_gzip(is_gzip);

                            send_msg.set_topic(topic);
                            send_msg.set_type(protocol::Message_Type_PUB);

                            if (is_gzip) {
                                std::vector<char> comp;
                                boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
                                out.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip::best_compression));
                                out.push(boost::iostreams::back_inserter(comp));
                                boost::iostreams::copy(boost::iostreams::basic_array_source<char>(&buf[0], buf.size()),
                                                       out);
                                send_msg.set_data(comp.data(), comp.size());
                            } else {
                                send_msg.set_data(buf.data(), buf.size());
                            }

                            int send_msg_size = send_msg.ByteSize();
                            Ptr<std::vector<char>> pub_buf(new std::vector<char>(send_msg_size));
                            send_msg.SerializeToArray(pub_buf->data(), send_msg_size);

                            session.send(pub_buf);
                        });
                map[session.session_id()] = sub_id;
            } else if (message.type() == protocol::Message_Type::Message_Type_UNSUB) {
            } else if (message.type() == protocol::Message_Type::Message_Type_PUB) {

            }
        });
        server.on_websocket_close([this, &map](WebsocketSession &session) {
            Logger::info("ws", "ws session close, id={}, sub_id={}, map={}", session.session_id(),
                         map[session.session_id()], map.size());
            DataBus::unsubscribe(map[session.session_id()]);
        });
        server.start(true);
    }
};

int main() {
    DataBusTest t;
    t.start();
}
