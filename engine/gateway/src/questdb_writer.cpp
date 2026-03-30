#include "efx/gateway/questdb_writer.hpp"
#include <spdlog/spdlog.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace efx {

namespace {

int64_t wall_clock_us(WallClock wc) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        wc.time_since_epoch()).count();
}

// Escape tag values for ILP (spaces, commas, equals)
std::string escape_tag(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == ' ' || c == ',' || c == '=') out += '\\';
        out += c;
    }
    return out;
}

} // anonymous namespace

QuestDbWriter::QuestDbWriter(Config config) : config_(std::move(config)) {}

QuestDbWriter::~QuestDbWriter() {
    stop();
}

void QuestDbWriter::start() {
    if (running_.exchange(true)) return;
    flush_thread_ = std::thread(&QuestDbWriter::flush_loop, this);
    spdlog::info("QuestDB writer started ({}:{})", config_.host, config_.port);
}

void QuestDbWriter::stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_one();
    if (flush_thread_.joinable()) flush_thread_.join();
    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }
    spdlog::info("QuestDB writer stopped. Lines written: {}, errors: {}",
        lines_written_.load(), errors_.load());
}

void QuestDbWriter::write_tick(const VenueTick& tick) {
    // ILP format: table,tag1=val1,tag2=val2 field1=val1,field2=val2 timestamp_us
    std::ostringstream oss;
    oss << std::setprecision(10)
        << "market_ticks"
        << ",venue=" << escape_tag(tick.venue.id)
        << ",pair=" << escape_tag(tick.pair)
        << " bid=" << tick.bid
        << ",ask=" << tick.ask
        << ",bid_size=" << tick.bid_size
        << ",ask_size=" << tick.ask_size
        << " " << wall_clock_us(tick.wall_time) * 1000 // nanoseconds for QuestDB
        << "\n";
    enqueue(oss.str());
}

void QuestDbWriter::write_fair_value(const FairValue& fv) {
    std::ostringstream oss;
    oss << std::setprecision(10)
        << "fair_values"
        << ",pair=" << escape_tag(fv.pair)
        << " mid=" << fv.mid
        << ",confidence=" << fv.confidence
        << ",volatility=" << fv.volatility
        << ",num_venues=" << fv.num_active_venues << "i"
        << " " << wall_clock_us(fv.wall_time) * 1000
        << "\n";
    enqueue(oss.str());
}

void QuestDbWriter::write_pnl(const PnlSnapshot& pnl) {
    std::ostringstream oss;
    oss << std::setprecision(10)
        << "pnl_snapshots"
        << ",pair=" << escape_tag(pnl.pair)
        << ",session=" << escape_tag(pnl.session)
        << " spread_pnl=" << pnl.spread_pnl
        << ",position_pnl=" << pnl.position_pnl
        << ",hedge_cost=" << pnl.hedge_cost
        << ",total_pnl=" << pnl.total_pnl
        << ",position=" << pnl.position
        << ",drawdown=" << pnl.drawdown
        << ",peak_pnl=" << pnl.peak_pnl
        << " " << wall_clock_us(pnl.wall_time) * 1000
        << "\n";
    enqueue(oss.str());
}

void QuestDbWriter::enqueue(std::string line) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(line));
    }
    if (queue_.size() >= config_.batch_size) {
        cv_.notify_one();
    }
}

bool QuestDbWriter::try_connect() {
    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }

    sock_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
        spdlog::warn("QuestDB: failed to create socket: {}", strerror(errno));
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);

    if (::connect(sock_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    connected_.store(true);
    spdlog::info("QuestDB: connected to {}:{}", config_.host, config_.port);
    return true;
}

bool QuestDbWriter::send_batch(const std::string& batch) {
    if (sock_fd_ < 0) return false;

    size_t total = batch.size();
    size_t sent = 0;
    while (sent < total) {
        ssize_t n = ::send(sock_fd_, batch.data() + sent, total - sent, 0);
        if (n <= 0) {
            spdlog::warn("QuestDB: send failed: {}", strerror(errno));
            connected_.store(false);
            ::close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }
        sent += n;
    }
    return true;
}

void QuestDbWriter::flush_loop() {
    while (running_.load()) {
        std::deque<std::string> batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(config_.flush_interval_ms),
                [this]{ return !queue_.empty() || !running_.load(); });
            if (queue_.empty()) continue;
            batch.swap(queue_);
        }

        if (!connected_.load()) {
            if (!try_connect()) {
                errors_.fetch_add(batch.size());
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }

        std::string combined;
        combined.reserve(batch.size() * 128);
        for (auto& line : batch) {
            combined += line;
        }

        if (send_batch(combined)) {
            lines_written_.fetch_add(batch.size());
        } else {
            errors_.fetch_add(batch.size());
        }
    }
}

} // namespace efx
