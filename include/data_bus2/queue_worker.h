#pragma once

#include "ring_queue.h"
#include "queue_stat.h"
#include "callback_holder.h"

namespace data_bus {

    class QueueWorker {
    public:
        QueueWorker(std::string topic, std::string callback_name, Ptr<CallbackHolder> callback_holder, int max_queue_size)
                : topic_(topic), callback_name_(callback_name), callback_holder_(std::move(callback_holder)),
                  queue_(max_queue_size) {
            worker_thread_ = std::make_shared<std::thread>([this] {
                while (!this->is_stop_) {
                    Ptr<ProtoMessage> data = this->queue_.take();
                    try {
                        TimeElapsed time;
                        this->callback_holder_->call(data);
                        cost_time_sec_ = time.elapsed();
                        total_time_sec_ += cost_time_sec_;
                        success_count_++;
                    } catch (std::exception &e) {
                        Logger::error("QueueWorker",
                                      "Data bus callback error, topic={}, callback_name={}, callback_id={}, error: ",
                                      this->topic_, this->callback_name_, callback_holder_->getCallbackId(),
                                      e.what());
                    }
                }
            });
            worker_thread_->detach();
        }

        ~QueueWorker() {
            is_stop_ = true;
            worker_thread_->join();
        }

        void putData(Ptr<ProtoMessage> data) {
            queue_.put(data);
        }

        long getCallbackId() {
            return callback_holder_->getCallbackId();
        }

        Ptr<CallbackStat> getCallbackStat() {
            Ptr<CallbackStat> stat(new CallbackStat);
            stat->topic = topic_;
            stat->callback_name = callback_name_;
            stat->callback_id = getCallbackId();
            stat->queue_size = queue_.size();
            stat->max_queue_size = queue_.maxSize();
            stat->incoming_count = queue_.incomingCount();
            stat->dropped_count = queue_.droppedCount();
            stat->success_count = static_cast<std::size_t >(success_count_);
            stat->cost_time_sec = cost_time_sec_;
            stat->total_time_sec = total_time_sec_;
            return stat;
        }

    private:
        std::atomic_bool is_stop_{false};
        std::string topic_;
        std::string callback_name_;
        RingQueue<Ptr<ProtoMessage>> queue_;
        Ptr<CallbackHolder> callback_holder_;
        std::shared_ptr<std::thread> worker_thread_;

        std::atomic_long success_count_{0};
        double cost_time_sec_{0};
        double total_time_sec_{0};
    };

}
