#pragma once

#include "efx/core/types.hpp"
#include <string>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace efx {

// Writes data to QuestDB via InfluxDB Line Protocol (ILP) over TCP.
// Batches writes for efficiency. Non-blocking: data is queued and
// flushed by a background thread.
class QuestDbWriter {
public:
    struct Config {
        std::string host = "127.0.0.1";
        int port = 9009;
        size_t batch_size = 1000;
        int flush_interval_ms = 100;
    };

    QuestDbWriter() : QuestDbWriter(Config{}) {}
    explicit QuestDbWriter(Config config);
    ~QuestDbWriter();

    QuestDbWriter(const QuestDbWriter&) = delete;
    QuestDbWriter& operator=(const QuestDbWriter&) = delete;

    void write_tick(const VenueTick& tick);
    void write_fair_value(const FairValue& fv);
    void write_pnl(const PnlSnapshot& pnl);

    bool is_connected() const { return connected_.load(); }
    uint64_t lines_written() const { return lines_written_.load(); }
    uint64_t errors() const { return errors_.load(); }

    void start();
    void stop();

private:
    void enqueue(std::string line);
    void flush_loop();
    bool try_connect();
    bool send_batch(const std::string& batch);

    Config config_;
    int sock_fd_ = -1;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> lines_written_{0};
    std::atomic<uint64_t> errors_{0};

    std::deque<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread flush_thread_;
};

} // namespace efx
