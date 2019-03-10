#pragma once


#include "data_bus/subscriber_worker.h"
#include "tcp_tool/tcp_client.h"
#include "Protocol.pb.h"
#include "util/proto_utils.h"
#include "util/zlib_utils.h"

namespace data_bus {

    using namespace tcp_tool;
    using namespace util;

    class DataBusClient {
    public:
        static const int DEFAULT_QUEUE_SIZE = 1;

        DataBusClient() = default;

        DataBusClient(const DataBus &) = delete;

        DataBusClient &operator=(const DataBus &) = delete;

        static void connect(const std::string &host, unsigned short port) {
            instance()->tcp_client_.encoder([](protocol::Message &t, std::vector<char> &data) {
                int msg_size = t.ByteSize();
                data.resize(msg_size);
                t.SerializeToArray(data.data(), msg_size);
            });

            instance()->tcp_client_.decoder([](std::vector<char> &data, protocol::Message &t) -> bool {
                t.ParseFromArray(data.data(), data.size());
                data.clear();
                return true;
            });

            instance()->tcp_client_.handler([&](protocol::Message &message, TcpSession<protocol::Message> &session) {
                std::vector<char> packed;
                if (message.compressed()) {
                    ZlibUtils::decompress(message.payload(), packed);
                } else {
                    packed.resize(message.payload().size());
                    packed.assign(message.payload().begin(), message.payload().end());
                }

                if (message.type() == protocol::Message_Type::Message_Type_SUB_ACK) {
                    protocol::SubAckPayload ack;
                    ack.ParseFromArray(message.payload().data(), message.payload().size());
                    Logger::info("DataBusClient", "Subscribe successfully, topic={}, subscriber_name={}.", ack.topic(),
                                 ack.subscriber_name());
                } else if (message.type() == protocol::Message_Type::Message_Type_UNSUB_ACK) {
                    protocol::UnSubAckPayload ack;
                    ack.ParseFromArray(message.payload().data(), message.payload().size());
                    if (instance()->subscriber_map_.count(ack.topic()) == 0) {
                        Logger::error("DataBusClient",
                                      "Can not find subscriber by topic, topic={}, subscriber_name={}.", ack.topic(),
                                      ack.subscriber_name());
                        return;
                    }

                    instance()->subscriber_map_.erase(ack.topic());
                    Logger::info("DataBusClient", "Unsubscribe successfully, topic={}, subscriber_name={}.",
                                 ack.topic(),
                                 ack.subscriber_name());
                } else if (message.type() == protocol::Message_Type::Message_Type_PUB) {
                    protocol::PubPayload pub;
                    pub.ParseFromArray(message.payload().data(), message.payload().size());
                    std::lock_guard<std::mutex> locker(instance()->mutex_);
                    if (instance()->subscriber_map_.count(pub.topic()) == 0) {
                        Logger::error("DataBusClient", "Can not find subscriber by topic, topic={}.", pub.topic());
                        return;
                    }

                    Ptr<ProtoMessage> msg_ptr(ProtoUtils::createMessage(pub.data_type()));
                    msg_ptr->ParseFromArray(pub.data().data(), pub.data().size());

                    instance()->subscriber_map_[pub.topic()]->putData(msg_ptr);
                }
            });

            instance()->tcp_client_.connect(host, port);
            instance()->is_connected_ = true;
        };

        template<typename T>
        static void publish(const std::string &topic, Ptr<T> data, bool compressed = false) {
            if (!instance()->is_connected_) {
                Logger::error("DataBusClient", "Tcp client is not connected, please call DataBusClient::connect.");
                return;
            }
            Ptr<ProtoMessage> msg = std::static_pointer_cast<ProtoMessage>(data);
            int size = msg->ByteSize();
            std::vector<char> buf(size);
            msg->SerializeToArray(buf.data(), size);

            protocol::PubPayload payload;
            payload.set_topic(topic);
            payload.set_data_type(msg->GetTypeName());
            payload.set_data(buf.data(), size);
            int payload_size = payload.ByteSize();
            std::vector<char> payload_buf(payload_size);
            payload.SerializeToArray(payload_buf.data(), payload_size);

            protocol::Message message;
            message.set_compressed(compressed);
            message.set_type(protocol::Message_Type_PUB);
            if (compressed) {
                std::vector<char> packed;
                ZlibUtils::compress(payload_buf, packed);
                message.set_payload(packed.data(), packed.size());
            } else {
                message.set_payload(payload_buf.data(), payload_size);
            }

            instance()->tcp_client_.send(message);
        }

        template<typename T>
        static bool subscribe(const std::string &topic, const std::string &subscriber_name,
                              const Callback<T> &callback, int max_queue_size = DEFAULT_QUEUE_SIZE,
                              bool compressed = false, int max_rate = 0) {
            if (!instance()->is_connected_) {
                Logger::error("DataBusClient", "Tcp client is not connected, please call DataBusClient::connect.");
                return false;
            }
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            if (instance()->subscriber_map_.count(topic) > 0) {
                Logger::error("DataBusClient", "Can not subscribe repeatedly, topic={}, subscriber_name={}.", topic,
                              subscriber_name);
                return false;
            }

            protocol::SubPayload msg;
            msg.set_topic(topic);
            msg.set_subscriber_name(subscriber_name);
            msg.set_max_rate(max_rate);
            msg.set_compressed(compressed);
            int size = msg.ByteSize();
            std::vector<char> buf(size);
            msg.SerializeToArray(buf.data(), size);

            protocol::Message message;
            message.set_compressed(false);
            message.set_type(protocol::Message_Type_SUB);
            message.set_payload(buf.data(), size);
            instance()->tcp_client_.send(message);

            Ptr<Subscriber> subscriber(new SubscriberT<T>(subscriber_name, callback));
            Ptr<SubscriberWorker> worker = std::make_shared<SubscriberWorker>(topic, subscriber, max_queue_size);
            instance()->subscriber_map_[topic] = worker;
            return true;
        }

        static bool unsubscribe(const std::string &topic, const std::string &subscriber_name) {
            if (!instance()->is_connected_) {
                Logger::error("DataBusClient", "Tcp client is not connected, please call DataBusClient::connect.");
                return false;
            }

            protocol::UnSubPayload msg;
            msg.set_topic(topic);
            msg.set_subscriber_name(subscriber_name);
            int size = msg.ByteSize();
            std::vector<char> buf(size);
            msg.SerializeToArray(buf.data(), size);

            protocol::Message message;
            message.set_compressed(false);
            message.set_type(protocol::Message_Type_SUB);
            message.set_payload(buf.data(), size);
            instance()->tcp_client_.send(message);
            return true;
        }

        static std::list<QueueStat> getQueueStats() {
            std::lock_guard<std::mutex> locker(instance()->mutex_);
            std::list<QueueStat> stats;
            for (auto &pair : instance()->subscriber_map_) {
                stats.push_back(pair.second->getQueueStat());
            }
            return stats;
        }

    private:
        static DataBusClient *instance() {
            static DataBusClient instance;
            return &instance;
        }

    private:
        std::atomic_bool is_connected_{false};
        TcpClient<protocol::Message> tcp_client_;

        std::mutex mutex_;
        std::condition_variable_any wait_cond_;
        std::map<std::string, Ptr<SubscriberWorker>> subscriber_map_;
    };
}
