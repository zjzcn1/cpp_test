#pragma once

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/copy.hpp>

#include "data_bus.h"
#include "tcp_tool/tcp_server.h"
#include "Protocol.pb.h"
#include "util/proto_utils.h"
#include "util/zlib_utils.h"

namespace data_bus {

    using namespace tcp_tool;
    using namespace util;

    class DataBusProxy {
    public:
        DataBusProxy() = default;

        DataBusProxy(const DataBusProxy &) = delete;

        DataBusProxy &operator=(const DataBusProxy &) = delete;

        static void listen(unsigned short port) {
            instance()->tcp_server_.encoder([](protocol::Message &t, std::vector<char> &data) {
                int msg_size = t.ByteSize();
                data.resize(msg_size);
                t.SerializeToArray(data.data(), msg_size);
            });

            instance()->tcp_server_.decoder([](std::vector<char> &data, protocol::Message &t) -> bool {
                t.ParseFromArray(data.data(), data.size());
                data.clear();
                return true;
            });

            instance()->tcp_server_.handler([&](protocol::Message &message, TcpSession<protocol::Message> &session) {
                std::vector<char> packed;
                if (message.compressed()) {
                    ZlibUtils::decompress(message.payload(), packed);
                } else {
                    packed.resize(message.payload().size());
                    packed.assign(message.payload().begin(), message.payload().end());
                }

                if (message.type() == protocol::Message_Type::Message_Type_SUB) {
                    protocol::SubPayload payload;
                    payload.ParseFromArray(packed.data(), packed.size());

                    std::string topic = payload.topic();
                    std::string subscriber_name = payload.subscriber_name();
                    bool compressed = payload.compressed();
                    bool success = DataBus::subscribe<ProtoMessage>(
                            topic,
                            subscriber_name,
                            [topic, subscriber_name, compressed, &session](ConstPtr<ProtoMessage> msg) {
                                int size = msg->ByteSize();
                                std::vector<char> buf(size);
                                msg->SerializeToArray(buf.data(), size);

                                protocol::PubPayload pub;
                                pub.set_topic(topic);
                                pub.set_data_type(msg->GetTypeName());
                                pub.set_data(buf.data(), size);
                                int pub_size = pub.ByteSize();
                                std::vector<char> pub_buf(pub_size);
                                pub.SerializeToArray(pub_buf.data(), pub_size);

                                protocol::Message message;
                                message.set_compressed(compressed);
                                message.set_type(protocol::Message_Type_PUB);
                                if (compressed) {
                                    std::vector<char> packed;
                                    ZlibUtils::compress(pub_buf, packed);
                                    message.set_payload(packed.data(), packed.size());
                                } else {
                                    message.set_payload(pub_buf.data(),pub_buf.size());
                                }

                                session.send(message);
                            });

                    protocol::SubAckPayload ack_payload;
                    ack_payload.set_topic(topic);
                    ack_payload.set_subscriber_name(subscriber_name);
                    int payload_size = ack_payload.ByteSize();
                    std::vector<char> payload_buf(payload_size);
                    if (success) {
                        ack_payload.set_result(protocol::AckResult::SUCCESS);
                    } else {
                        ack_payload.set_result(protocol::AckResult::SUB_REPEATED);
                    }
                    ack_payload.SerializeToArray(payload_buf.data(), payload_size);

                    protocol::Message ack;
                    ack.set_compressed(false);
                    ack.set_type(protocol::Message_Type_SUB_ACK);
                    ack.set_payload(payload_buf.data(), payload_size);

                    session.send(ack);
                } else if (message.type() == protocol::Message_Type::Message_Type_UNSUB) {
                    protocol::SubPayload payload;
                    payload.ParseFromArray(packed.data(), packed.size());

                    std::string topic = payload.topic();
                    std::string subscriber_name = payload.subscriber_name();
                    bool compressed = payload.compressed();
                    bool success = DataBus::unsubscribe(topic, subscriber_name);

                    protocol::SubAckPayload ack_payload;
                    ack_payload.set_topic(topic);
                    ack_payload.set_subscriber_name(subscriber_name);
                    int payload_size = ack_payload.ByteSize();
                    std::vector<char> payload_buf(payload_size);
                    if (success) {
                        ack_payload.set_result(protocol::AckResult::SUCCESS);
                    } else {
                        ack_payload.set_result(protocol::AckResult::UNSUB_NOT_FOUND);
                    }
                    ack_payload.SerializeToArray(payload_buf.data(), payload_size);

                    protocol::Message ack;
                    ack.set_compressed(false);
                    ack.set_type(protocol::Message_Type_UNSUB_ACK);
                    ack.set_payload(payload_buf.data(), payload_size);

                    session.send(ack);
                } else if (message.type() == protocol::Message_Type::Message_Type_PUB) {
                    protocol::PubPayload pub;
                    pub.ParseFromArray(packed.data(), packed.size());

                    std::string topic = pub.topic();
                    std::string d(packed.begin(), packed.end());
                    Ptr<ProtoMessage> msg_ptr(ProtoUtils::createMessage(pub.data_type()));
                    msg_ptr->ParseFromArray(pub.data().data(), pub.data().size());

                    DataBus::publish<ProtoMessage>(topic, msg_ptr);
                }
            });

            instance()->tcp_server_.listen(port);
        };

    private:
        static DataBusProxy *instance() {
            static DataBusProxy instance;
            return &instance;
        }

    private:
        TcpServer<protocol::Message> tcp_server_;
    };
}
