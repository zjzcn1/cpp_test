#include <iostream>
#include <string>
#include <map>
#include "Message.pb.h"
#include "Pose.pb.h"

template<typename T>
using Callback = std::function<void(std::shared_ptr<T const>)>;

class Handler {
};

using HandlerPtr = std::shared_ptr<Handler>;

template<typename T>
class HandlerImpl : public Handler {
private:
    const Callback<T> &callback_;
public:
    explicit HandlerImpl(const Callback<T> &callback) : callback_(callback) {

    }

    Callback<T> getCallback() {
        return callback_;
    }
};


template<class F>
struct function_traits;

template<class R, class... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;

    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    };
};

// function pointer
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};

struct Test {

    std::map<std::string, HandlerPtr> map;

    void onEvent(std::shared_ptr<msg::Pose const> e) {
        std::cout << "--------------time: " << e->name() << std::endl;
    }

    void start() {
        HandlerPtr ptr(new HandlerImpl<msg::Pose>(std::bind(&Test::onEvent, this, std::placeholders::_1)));
        map["pose"] = ptr;

        msg::Pose pose;
        pose.set_id(1);
        pose.set_name("hehe");
        msg::Message message{};
        message.set_topic("pose");
        std::string buff{};
        pose.SerializeToString(&buff);
        message.set_data(buff);
        std::string buff1{};
        message.SerializeToString(&buff1);

        msg::Message message1;
        message1.ParseFromString(buff1);
        HandlerPtr handler = map[message1.topic()];
//        HandlerImpl<T> *impl = static_cast<QueueWorkerImpl<T> *>(worker_ptr.get());
//        using Traits = function_traits<decltype(handler)>;
        function_traits::
        std::cout<<message1.topic()<<std::endl;
    }
};

int main()
{
    Test t;
    t.start();
}

