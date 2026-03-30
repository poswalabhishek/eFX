#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/core/ring_buffer.hpp"
#include "efx/market_data/simulator.hpp"
#include "efx/market_data/venue_book.hpp"
#include "efx/pricing/fair_value.hpp"
#include "efx/pricing/volatility.hpp"
#include "efx/pricing/spread_constructor.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <iostream>
#include <iomanip>

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
    if (argc > 1) {
        config_dir = argv[1];
    }

    spdlog::info("=== EFX Pricing Engine v0.1.0 ===");
    spdlog::info("Loading config from: {}", config_dir);

    auto config = efx::load_config(config_dir);

    efx::MarketDataSimulator simulator(config);
    efx::VenueBookManager book(config);
    efx::FairValueEngine fair_value(config);
    efx::VolatilityEstimator vol_estimator;
    efx::SpreadConstructor spread_ctor(config);

    simulator.set_time_step_ms(1.0);

    // Tick/price counters for logging
    uint64_t total_ticks = 0;
    uint64_t total_steps = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_log_time = start_time;

    spdlog::info("Starting pricing engine loop...");

    while (g_running.load(std::memory_order_relaxed)) {
        auto step_start = std::chrono::steady_clock::now();

        // Generate market data ticks
        simulator.step([&](const efx::VenueTick& tick) {
            book.on_tick(tick);
            total_ticks++;
        });

        // Update staleness
        book.check_staleness(std::chrono::steady_clock::now());

        // Compute fair values
        fair_value.update(book, std::chrono::steady_clock::now());

        // Update volatility and log fair values
        for (const auto& [pair, fv] : fair_value.all()) {
            if (fv.mid > 0.0) {
                vol_estimator.on_fair_value(pair, fv.mid, fv.timestamp);
            }
        }

        total_steps++;

        // Periodic logging (every second)
        auto now = std::chrono::steady_clock::now();
        auto since_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
        if (since_log.count() >= 1) {
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time).count() / 1000.0;
            double tps = total_ticks / elapsed;

            spdlog::info("Steps: {} | Ticks: {} ({:.0f}/s)", total_steps, total_ticks, tps);

            // Print fair values for a few key pairs
            for (const auto& sym : {"EURUSD", "USDJPY", "GBPUSD"}) {
                auto fv = fair_value.get(sym);
                if (fv && fv->mid > 0.0) {
                    double vol = vol_estimator.get_volatility(sym);
                    int decimals = (std::string(sym).find("JPY") != std::string::npos) ? 3 : 5;
                    spdlog::info("  {} mid={:.{}f} conf={:.2f} vol={:.4f} venues={}",
                        sym, fv->mid, decimals, fv->confidence, vol, fv->num_active_venues);
                }
            }

            last_log_time = now;
        }

        // Pace simulation: ~1000 steps/sec = 1ms simulated per 1ms wall clock
        auto step_end = std::chrono::steady_clock::now();
        auto step_dur = std::chrono::duration_cast<std::chrono::microseconds>(step_end - step_start);
        if (step_dur.count() < 1000) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000 - step_dur.count()));
        }
    }

    spdlog::info("EFX engine shutting down. Total ticks processed: {}", total_ticks);
    return 0;
}
