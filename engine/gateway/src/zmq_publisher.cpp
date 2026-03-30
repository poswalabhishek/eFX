#include "efx/gateway/zmq_publisher.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace efx {

ZmqPublisher::ZmqPublisher(const std::string& bind_address)
    : context_(1), socket_(context_, zmq::socket_type::pub) {
    socket_.set(zmq::sockopt::sndhwm, 10000);
    socket_.set(zmq::sockopt::linger, 0);
    socket_.bind(bind_address);
    spdlog::info("ZMQ publisher bound to {}", bind_address);
}

ZmqPublisher::~ZmqPublisher() {
    socket_.close();
    context_.close();
}

void ZmqPublisher::publish_tick(const VenueTick& tick) {
    nlohmann::json j = {
        {"type", "tick"},
        {"venue", tick.venue.id},
        {"pair", tick.pair},
        {"bid", tick.bid},
        {"ask", tick.ask},
        {"mid", tick.mid()},
        {"spread", tick.spread()},
        {"bid_size", tick.bid_size},
        {"ask_size", tick.ask_size},
        {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
            tick.wall_time.time_since_epoch()).count()},
    };
    publish("tick." + tick.pair, j);
}

void ZmqPublisher::publish_fair_value(const FairValue& fv) {
    nlohmann::json j = {
        {"type", "fair_value"},
        {"pair", fv.pair},
        {"mid", fv.mid},
        {"confidence", fv.confidence},
        {"volatility", fv.volatility},
        {"num_venues", fv.num_active_venues},
        {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
            fv.wall_time.time_since_epoch()).count()},
    };
    publish("fair_value." + fv.pair, j);
}

void ZmqPublisher::publish_pnl(const PnlSnapshot& pnl) {
    nlohmann::json j = {
        {"type", "pnl"},
        {"pair", pnl.pair},
        {"session", pnl.session},
        {"spread_pnl", pnl.spread_pnl},
        {"position_pnl", pnl.position_pnl},
        {"hedge_cost", pnl.hedge_cost},
        {"total_pnl", pnl.total_pnl},
        {"position", pnl.position},
        {"drawdown", pnl.drawdown},
        {"peak_pnl", pnl.peak_pnl},
        {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
            pnl.wall_time.time_since_epoch()).count()},
    };
    publish("pnl", j);
}

void ZmqPublisher::publish(const std::string& topic, const nlohmann::json& data) {
    std::string payload = data.dump();

    zmq::message_t topic_msg(topic.data(), topic.size());
    zmq::message_t data_msg(payload.data(), payload.size());

    socket_.send(topic_msg, zmq::send_flags::sndmore);
    socket_.send(data_msg, zmq::send_flags::none);
    messages_sent_++;
}

} // namespace efx
