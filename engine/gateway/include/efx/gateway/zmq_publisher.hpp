#pragma once

#include "efx/core/types.hpp"
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace efx {

// Publishes engine state over ZMQ PUB socket for Python consumers.
// Messages are JSON with a topic prefix for filtering.
//
// Topics:
//   "tick.<pair>"       - raw market data ticks
//   "fair_value.<pair>" - fair value updates
//   "price.<pair>"      - constructed client prices
//   "position.<pair>"   - position updates
//   "pnl"               - PnL snapshots
class ZmqPublisher {
public:
    explicit ZmqPublisher(const std::string& bind_address = "tcp://*:5555");
    ~ZmqPublisher();

    ZmqPublisher(const ZmqPublisher&) = delete;
    ZmqPublisher& operator=(const ZmqPublisher&) = delete;

    void publish_tick(const VenueTick& tick);
    void publish_fair_value(const FairValue& fv);
    void publish_pnl(const PnlSnapshot& pnl);

    // Generic topic + JSON publish
    void publish(const std::string& topic, const nlohmann::json& data);

    uint64_t messages_sent() const { return messages_sent_; }

private:
    zmq::context_t context_;
    zmq::socket_t socket_;
    uint64_t messages_sent_ = 0;
};

} // namespace efx
