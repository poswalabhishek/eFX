#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/core/ring_buffer.hpp"
#include "efx/market_data/simulator.hpp"
#include "efx/market_data/venue_book.hpp"
#include "efx/pricing/fair_value.hpp"
#include "efx/pricing/volatility.hpp"
#include "efx/pricing/spread_constructor.hpp"
#include "efx/gateway/zmq_publisher.hpp"
#include "efx/gateway/questdb_writer.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto logger = spdlog::stdout_color_mt("efx");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

    std::string config_dir = "./config";
    if (argc > 1) config_dir = argv[1];

    spdlog::info("=== EFX Pricing Engine v0.2.0 ===");

    auto config = efx::load_config(config_dir);

    // Core components
    efx::MarketDataSimulator simulator(config);
    efx::VenueBookManager book(config);
    efx::FairValueEngine fair_value(config);
    efx::VolatilityEstimator vol_estimator;
    efx::SpreadConstructor spread_ctor(config);

    // Data pipeline
    efx::ZmqPublisher zmq_pub("tcp://*:5555");

    efx::QuestDbWriter::Config qdb_config;
    qdb_config.host = "127.0.0.1";
    qdb_config.port = 9009;
    qdb_config.batch_size = 500;
    qdb_config.flush_interval_ms = 200;
    efx::QuestDbWriter qdb_writer(qdb_config);
    qdb_writer.start();

    simulator.set_time_step_ms(1.0);

    uint64_t total_ticks = 0;
    uint64_t total_steps = 0;
    uint64_t zmq_fair_value_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_log_time = start_time;
    auto last_fv_publish = start_time;

    // Throttle fair value ZMQ publishing to ~20 Hz (every 50ms)
    constexpr auto fv_publish_interval = std::chrono::milliseconds(50);

    spdlog::info("Starting pricing engine loop (ZMQ on tcp://*:5555)...");

    while (g_running.load(std::memory_order_relaxed)) {
        auto step_start = std::chrono::steady_clock::now();

        // Generate and ingest market data
        simulator.step([&](const efx::VenueTick& tick) {
            book.on_tick(tick);
            qdb_writer.write_tick(tick);
            total_ticks++;
        });

        book.check_staleness(std::chrono::steady_clock::now());
        fair_value.update(book, std::chrono::steady_clock::now());

        // Update volatility
        for (const auto& [pair, fv] : fair_value.all()) {
            if (fv.mid > 0.0) {
                vol_estimator.on_fair_value(pair, fv.mid, fv.timestamp);
            }
        }

        total_steps++;

        // Throttled fair value publishing via ZMQ
        auto now = std::chrono::steady_clock::now();
        if (now - last_fv_publish >= fv_publish_interval) {
            for (const auto& [pair, fv] : fair_value.all()) {
                if (fv.mid <= 0.0) continue;

                // Enrich with volatility
                efx::FairValue enriched = fv;
                enriched.volatility = vol_estimator.get_volatility(pair);

                zmq_pub.publish_fair_value(enriched);
                qdb_writer.write_fair_value(enriched);
                zmq_fair_value_count++;
            }
            last_fv_publish = now;
        }

        // Periodic logging (every 2 seconds)
        auto since_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
        if (since_log.count() >= 2) {
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time).count() / 1000.0;
            double tps = total_ticks / elapsed;

            spdlog::info("Steps:{} Ticks:{} ({:.0f}/s) ZMQ:{} QDB:{}/{}err",
                total_steps, total_ticks, tps,
                zmq_pub.messages_sent(),
                qdb_writer.lines_written(), qdb_writer.errors());

            for (const auto& sym : {"EURUSD", "USDJPY", "GBPUSD"}) {
                auto fv = fair_value.get(sym);
                if (fv && fv->mid > 0.0) {
                    double vol = vol_estimator.get_volatility(sym);
                    int dec = (std::string(sym).find("JPY") != std::string::npos) ? 3 : 5;
                    spdlog::info("  {} mid={:.{}f} conf={:.2f} vol={:.4f} venues={}",
                        sym, fv->mid, dec, fv->confidence, vol, fv->num_active_venues);
                }
            }
            last_log_time = now;
        }

        // Pace: ~1000 steps/sec
        auto step_end = std::chrono::steady_clock::now();
        auto step_dur = std::chrono::duration_cast<std::chrono::microseconds>(step_end - step_start);
        if (step_dur.count() < 1000) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000 - step_dur.count()));
        }
    }

    spdlog::info("Shutting down. Ticks:{} ZMQ:{} QDB:{}", total_ticks,
        zmq_pub.messages_sent(), qdb_writer.lines_written());

    qdb_writer.stop();
    return 0;
}
