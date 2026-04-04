#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/market_data/simulator.hpp"
#include "efx/market_data/venue_book.hpp"
#include "efx/pricing/fair_value.hpp"
#include "efx/pricing/volatility.hpp"
#include "efx/pricing/client_pricer.hpp"
#include "efx/order/order_manager.hpp"
#include "efx/order/client_simulator.hpp"
#include "efx/hedger/auto_hedger.hpp"
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
void signal_handler(int) { g_running.store(false, std::memory_order_relaxed); }

static std::vector<efx::ClientPricingConfig> seed_clients() {
    using T = efx::TierId;
    return {
        {.client_id="hedge_fund_alpha",  .name="Alpha Capital",     .default_tier=T::Gold,       .alpha_score=2.1,  .is_active=true},
        {.client_id="real_money_pension",.name="Global Pension",    .default_tier=T::Platinum,   .alpha_score=-0.8, .is_active=true},
        {.client_id="corp_treasury_acme",.name="Acme Treasury",     .default_tier=T::Silver,     .alpha_score=-0.3, .is_active=true},
        {.client_id="toxic_hft_firm",    .name="Velocity Trading",  .default_tier=T::Restricted, .alpha_score=5.2,  .is_active=true},
        {.client_id="retail_aggregator", .name="FX Connect Retail", .default_tier=T::Silver,     .alpha_score=0.1,  .is_active=true},
        {.client_id="sovereign_fund",    .name="Norges Investment", .default_tier=T::Platinum,   .alpha_score=-0.5, .is_active=true},
        {.client_id="algo_firm_momentum",.name="Quant Momentum",   .default_tier=T::Bronze,     .alpha_score=3.8,  .is_active=true},
        {.client_id="bank_flow_dealer",  .name="Regional Bank",    .default_tier=T::Gold,       .alpha_score=0.3,  .is_active=true},
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

    spdlog::info("=== EFX Pricing Engine v0.5.0 ===");
    auto config = efx::load_config(config_dir);

    // Core components
    efx::MarketDataSimulator simulator(config);
    efx::VenueBookManager book(config);
    efx::FairValueEngine fair_value(config);
    efx::VolatilityEstimator vol_estimator;
    efx::ClientPricer client_pricer(config);
    client_pricer.load_clients(seed_clients());

    // Order management + hedging + PnL
    efx::PositionManager pos_mgr;
    efx::OrderManager oms(config, pos_mgr);
    efx::AutoHedger hedger(config);
    efx::PnlEngine pnl_engine;
    efx::ClientSimulator client_sim(oms, client_pricer);

    // Data pipeline
    efx::ZmqPublisher zmq_pub("tcp://*:5555");
    efx::QuestDbWriter::Config qdb_config;
    qdb_config.batch_size = 500;
    qdb_config.flush_interval_ms = 200;
    efx::QuestDbWriter qdb_writer(qdb_config);
    qdb_writer.start();

    // Wire fill -> hedge -> PnL pipeline
    oms.set_fill_callback([&](const efx::Fill& fill) {
        pnl_engine.on_client_fill(fill);
        hedger.on_client_fill(fill, book);

        nlohmann::json j = {
            {"type", "fill"},
            {"trade_id", fill.trade_id},
            {"client_id", fill.client_id.id},
            {"pair", fill.pair},
            {"side", std::string(efx::to_string(fill.side))},
            {"price", fill.price},
            {"amount", fill.amount},
            {"venue", fill.venue.id},
            {"mid_at_fill", fill.mid_at_fill},
            {"spread_bps", fill.spread_bps},
            {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
                fill.wall_time.time_since_epoch()).count()},
        };
        zmq_pub.publish("fill." + fill.pair, j);
    });

    hedger.set_hedge_callback([&](const efx::HedgeExecution& hedge) {
        pnl_engine.on_hedge(hedge);
        pos_mgr.on_fill(efx::Fill{
            .trade_id = hedge.hedge_id,
            .quote_id = 0,
            .client_id = efx::ClientId{"_hedger"},
            .pair = hedge.pair,
            .side = hedge.side,
            .price = hedge.price,
            .amount = hedge.amount,
            .venue = hedge.venue,
            .mid_at_fill = hedge.price,
            .spread_bps = 0.0,
            .session = "london",
            .timestamp = hedge.timestamp,
            .wall_time = hedge.wall_time,
        });

        nlohmann::json j = {
            {"type", "hedge"},
            {"hedge_id", hedge.hedge_id},
            {"triggering_trade", hedge.triggering_trade_id},
            {"pair", hedge.pair},
            {"side", std::string(efx::to_string(hedge.side))},
            {"price", hedge.price},
            {"amount", hedge.amount},
            {"venue", hedge.venue.id},
            {"slippage_bps", hedge.slippage_bps},
            {"latency_us", hedge.latency_us},
        };
        zmq_pub.publish("hedge." + hedge.pair, j);
    });

    simulator.set_time_step_ms(1.0);

    uint64_t total_ticks = 0;
    uint64_t total_steps = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_log_time = start_time;
    auto last_fv_publish = start_time;
    auto last_client_publish = start_time;
    auto last_position_publish = start_time;

    constexpr auto fv_interval = std::chrono::milliseconds(50);
    constexpr auto client_interval = std::chrono::milliseconds(200);
    constexpr auto position_interval = std::chrono::milliseconds(500);

    spdlog::info("Starting engine loop ({} clients, OMS active, ZMQ on tcp://*:5555)...",
        client_pricer.client_count());

    while (g_running.load(std::memory_order_relaxed)) {
        auto step_start = std::chrono::steady_clock::now();

        // Market data
        simulator.step([&](const efx::VenueTick& tick) {
            book.on_tick(tick);
            qdb_writer.write_tick(tick);
            total_ticks++;
        });

        book.check_staleness(std::chrono::steady_clock::now());
        fair_value.update(book, std::chrono::steady_clock::now());

        // Volatility
        for (const auto& [pair, fv] : fair_value.all()) {
            if (fv.mid > 0.0) {
                vol_estimator.on_fair_value(pair, fv.mid, fv.timestamp);
                pos_mgr.mark_to_market(pair, fv.mid);
            }
        }

        // Client simulation: generate orders
        client_sim.step(fair_value, simulator.true_mid("EURUSD"), 1.0);

        total_steps++;
        auto now = std::chrono::steady_clock::now();

        // Publish fair values at 20 Hz
        if (now - last_fv_publish >= fv_interval) {
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
        if (now - last_client_publish >= client_interval) {
            for (const auto& [pair, fv] : fair_value.all()) {
                if (fv.mid <= 0.0) continue;
                double vol = vol_estimator.get_volatility(pair);
                double pos = pos_mgr.net_position_usd(pair);

                auto prices = client_pricer.price_all_clients(
                    pair, fv.mid, vol, pos, 0.0, 1.0);

                for (const auto& cp : prices) {
                    nlohmann::json j = {
                        {"type", "client_price"},
                        {"client_id", cp.client_id},
                        {"pair", pair},
                        {"bid", cp.bid},
                        {"ask", cp.ask},
                        {"spread_bps", cp.spread_bps},
                        {"tier", std::string(efx::tier_name(cp.tier))},
                    };
                    zmq_pub.publish("client_price." + pair, j);
                }
            }
            last_client_publish = now;
        }

        // Publish positions at 2 Hz
        if (now - last_position_publish >= position_interval) {
            auto positions = pos_mgr.get_all_positions();
            nlohmann::json pos_json = nlohmann::json::array();
            for (const auto& p : positions) {
                pos_json.push_back({
                    {"pair", p.pair},
                    {"position", p.position},
                    {"avg_entry", p.avg_entry_price},
                    {"realized_pnl", p.realized_pnl},
                    {"unrealized_pnl", p.unrealized_pnl},
                    {"trade_count", p.trade_count},
                });
            }

            nlohmann::json snapshot = {
                {"type", "positions"},
                {"positions", pos_json},
                {"total_realized_pnl", pos_mgr.total_realized_pnl()},
                {"total_unrealized_pnl", pos_mgr.total_unrealized_pnl()},
                {"total_fills", oms.total_fills()},
                {"total_rejects", oms.total_rejects()},
                {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
            };
            zmq_pub.publish("positions", snapshot);

            // Decomposed PnL snapshot
            pnl_engine.set_position_pnl(pos_mgr.total_realized_pnl(),
                                        pos_mgr.total_unrealized_pnl());
            auto pnl_bd = pnl_engine.breakdown();
            auto hedge_stats = hedger.stats();

            nlohmann::json pnl = {
                {"type", "engine_pnl"},
                {"spread_capture", pnl_bd.spread_capture},
                {"hedge_cost", pnl_bd.hedge_cost},
                {"realized_pnl", pnl_bd.realized_pnl},
                {"unrealized_pnl", pnl_bd.unrealized_pnl},
                {"net_pnl", pnl_bd.net_pnl()},
                {"total_fills", oms.total_fills()},
                {"total_hedges", hedge_stats.total_hedges},
                {"hedge_slippage_usd", hedge_stats.total_slippage_usd},
                {"avg_hedge_latency_us", hedge_stats.avg_latency_us},
                {"avg_hedge_slippage_bps", hedge_stats.avg_slippage_bps},
            };
            zmq_pub.publish("pnl", pnl);

            last_position_publish = now;
        }

        // Periodic logging
        auto since_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
        if (since_log.count() >= 3) {
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time).count() / 1000.0;

            auto bd = pnl_engine.breakdown();
            auto hs = hedger.stats();
            spdlog::info("Ticks:{} ({:.0f}/s) Fills:{} Hedges:{} | spread=${:.0f} hedge=${:.0f} net=${:.0f} | slip={:.2f}bps",
                total_ticks, total_ticks / elapsed,
                oms.total_fills(), hs.total_hedges,
                bd.spread_capture, bd.hedge_cost, bd.net_pnl(),
                hs.avg_slippage_bps);

            last_log_time = now;
        }

        auto step_end = std::chrono::steady_clock::now();
        auto step_dur = std::chrono::duration_cast<std::chrono::microseconds>(step_end - step_start);
        if (step_dur.count() < 1000) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000 - step_dur.count()));
        }
    }

    spdlog::info("Shutdown. Fills:{} Realized PnL:${:.0f}", oms.total_fills(),
        pos_mgr.total_realized_pnl());
    qdb_writer.stop();
    return 0;
}
