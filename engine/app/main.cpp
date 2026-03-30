#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/core/ring_buffer.hpp"
#include "efx/market_data/simulator.hpp"
#include "efx/market_data/venue_book.hpp"
#include "efx/pricing/fair_value.hpp"
#include "efx/pricing/volatility.hpp"
#include "efx/pricing/spread_constructor.hpp"
#include "efx/pricing/client_pricer.hpp"
#include "efx/gateway/zmq_publisher.hpp"
#include "efx/gateway/questdb_writer.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false, std::memory_order_relaxed);
}

static std::vector<efx::ClientPricingConfig> seed_clients() {
    using T = efx::TierId;
    return {
        {.client_id = "hedge_fund_alpha",   .name = "Alpha Capital Partners",  .default_tier = T::Gold,       .alpha_score = 2.1, .is_active = true},
        {.client_id = "real_money_pension",  .name = "Global Pension Fund",     .default_tier = T::Platinum,   .alpha_score = -0.8, .is_active = true},
        {.client_id = "corp_treasury_acme",  .name = "Acme Corp Treasury",      .default_tier = T::Silver,     .alpha_score = -0.3, .is_active = true},
        {.client_id = "toxic_hft_firm",      .name = "Velocity Trading LLC",    .default_tier = T::Restricted, .alpha_score = 5.2, .is_active = true},
        {.client_id = "retail_aggregator",   .name = "FX Connect Retail",       .default_tier = T::Silver,     .alpha_score = 0.1, .is_active = true},
        {.client_id = "sovereign_fund",      .name = "Norges Investment Mgmt",  .default_tier = T::Platinum,   .alpha_score = -0.5, .is_active = true},
        {.client_id = "algo_firm_momentum",  .name = "Quant Momentum Capital",  .default_tier = T::Bronze,     .alpha_score = 3.8, .is_active = true},
        {.client_id = "bank_flow_dealer",    .name = "Regional Bank Treasury",  .default_tier = T::Gold,       .alpha_score = 0.3, .is_active = true},
    };
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

    spdlog::info("=== EFX Pricing Engine v0.3.0 ===");

    auto config = efx::load_config(config_dir);

    efx::MarketDataSimulator simulator(config);
    efx::VenueBookManager book(config);
    efx::FairValueEngine fair_value(config);
    efx::VolatilityEstimator vol_estimator;
    efx::ClientPricer client_pricer(config);

    client_pricer.load_clients(seed_clients());

    efx::ZmqPublisher zmq_pub("tcp://*:5555");

    efx::QuestDbWriter::Config qdb_config;
    qdb_config.batch_size = 500;
    qdb_config.flush_interval_ms = 200;
    efx::QuestDbWriter qdb_writer(qdb_config);
    qdb_writer.start();

    simulator.set_time_step_ms(1.0);

    uint64_t total_ticks = 0;
    uint64_t total_steps = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_log_time = start_time;
    auto last_fv_publish = start_time;
    auto last_client_publish = start_time;

    constexpr auto fv_publish_interval = std::chrono::milliseconds(50);
    constexpr auto client_publish_interval = std::chrono::milliseconds(200);

    spdlog::info("Starting pricing engine loop ({} clients, ZMQ on tcp://*:5555)...",
        client_pricer.client_count());

    while (g_running.load(std::memory_order_relaxed)) {
        auto step_start = std::chrono::steady_clock::now();

        simulator.step([&](const efx::VenueTick& tick) {
            book.on_tick(tick);
            qdb_writer.write_tick(tick);
            total_ticks++;
        });

        book.check_staleness(std::chrono::steady_clock::now());
        fair_value.update(book, std::chrono::steady_clock::now());

        for (const auto& [pair, fv] : fair_value.all()) {
            if (fv.mid > 0.0) {
                vol_estimator.on_fair_value(pair, fv.mid, fv.timestamp);
            }
        }

        total_steps++;
        auto now = std::chrono::steady_clock::now();

        // Publish fair values at 20 Hz
        if (now - last_fv_publish >= fv_publish_interval) {
            for (const auto& [pair, fv] : fair_value.all()) {
                if (fv.mid <= 0.0) continue;
                efx::FairValue enriched = fv;
                enriched.volatility = vol_estimator.get_volatility(pair);
                zmq_pub.publish_fair_value(enriched);
                qdb_writer.write_fair_value(enriched);
            }
            last_fv_publish = now;
        }

        // Publish per-client prices at 5 Hz
        if (now - last_client_publish >= client_publish_interval) {
            for (const auto& [pair, fv] : fair_value.all()) {
                if (fv.mid <= 0.0) continue;
                double vol = vol_estimator.get_volatility(pair);

                auto client_prices = client_pricer.price_all_clients(
                    pair, fv.mid, vol, 0.0, 0.0, 1.0);

                for (const auto& cp : client_prices) {
                    nlohmann::json j = {
                        {"type", "client_price"},
                        {"client_id", cp.client_id},
                        {"pair", pair},
                        {"bid", cp.bid},
                        {"ask", cp.ask},
                        {"spread_bps", cp.spread_bps},
                        {"tier", std::string(efx::tier_name(cp.tier))},
                        {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
                            fv.wall_time.time_since_epoch()).count()},
                    };
                    zmq_pub.publish("client_price." + pair, j);
                }
            }
            last_client_publish = now;
        }

        // Periodic logging
        auto since_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
        if (since_log.count() >= 2) {
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time).count() / 1000.0;

            spdlog::info("Steps:{} Ticks:{} ({:.0f}/s) ZMQ:{} QDB:{}/{}err Clients:{}",
                total_steps, total_ticks, total_ticks / elapsed,
                zmq_pub.messages_sent(),
                qdb_writer.lines_written(), qdb_writer.errors(),
                client_pricer.client_count());

            auto fv_eu = fair_value.get("EURUSD");
            if (fv_eu && fv_eu->mid > 0.0) {
                auto prices = client_pricer.price_all_clients(
                    "EURUSD", fv_eu->mid, vol_estimator.get_volatility("EURUSD"),
                    0.0, 0.0, 1.0);
                if (!prices.empty()) {
                    double tightest = 999.0, widest = 0.0;
                    for (const auto& p : prices) {
                        tightest = std::min(tightest, p.spread_bps);
                        widest = std::max(widest, p.spread_bps);
                    }
                    spdlog::info("  EURUSD mid={:.5f} spreads=[{:.2f} - {:.2f}] bps ({} clients)",
                        fv_eu->mid, tightest, widest, prices.size());
                }
            }
            last_log_time = now;
        }

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
