#include "efx/pricing/spread_constructor.hpp"
#include <cmath>
#include <algorithm>

namespace efx {

SpreadConstructor::SpreadConstructor(const AppConfig& config) : config_(config) {}

SpreadConstructor::SpreadOutput SpreadConstructor::compute(const SpreadInput& input) const {
    auto pair_it = config_.pair_map.find(input.pair);
    if (pair_it == config_.pair_map.end()) {
        return {input.fair_value_mid, input.fair_value_mid, 0.0, 0.0};
    }

    const auto& pair_cfg = *pair_it->second;
    double pip = pair_cfg.pip_size;

    // Step 1: Base spread (in price units)
    double base_spread = pair_cfg.base_spread_pips * pip;

    // Step 2: Volatility adjustment
    //   spread_vol = base * (1 + k * vol / vol_baseline)
    double vol_adj = 1.0;
    if (input.volatility > 0.0 && config_.risk.volatility_baseline > 0.0) {
        double vol_ratio = input.volatility / config_.risk.volatility_baseline;
        vol_adj = 1.0 + config_.risk.volatility_coefficient * std::max(0.0, vol_ratio - 1.0);
    }
    double spread_after_vol = base_spread * vol_adj;

    // Step 3: Inventory skew
    //   Skew bid/ask to encourage offsetting flow
    double inventory_skew = 0.0;
    if (std::abs(input.position) > 0.0) {
        double max_pos = config_.risk.max_per_pair_usd;
        double norm_pos = std::clamp(input.position / max_pos, -1.0, 1.0);
        inventory_skew = config_.risk.skew_factor * norm_pos * pip;
        double max_skew = config_.risk.max_skew_bps * pip * 0.01 * input.fair_value_mid;
        inventory_skew = std::clamp(inventory_skew, -max_skew, max_skew);
    }

    // Step 4: Tier multiplier
    double tier_mult = tier_multiplier(input.tier);
    double spread_after_tier = spread_after_vol * tier_mult;

    // Step 5: Session multiplier
    double spread_after_session = spread_after_tier * input.session_multiplier;

    // Step 6: Client alpha skew (asymmetric widening)
    double alpha_skew = 0.0;
    if (std::abs(input.client_alpha) > 0.5) {
        // Toxic client: widen in the direction they're likely to trade
        alpha_skew = input.client_alpha * 0.3 * pip;
    }

    // Step 7: Pair alpha (directional signal skew)
    double pair_skew = 0.0;
    if (std::abs(input.pair_alpha) > 0.0) {
        pair_skew = input.pair_alpha * pip * 0.1;
    }

    // Step 8: Client+pair override
    double override_mult = 1.0 + (input.override_pct / 100.0);
    double final_spread = spread_after_session * std::max(override_mult, 0.1);
    double half_spread = final_spread / 2.0;

    // Apply skews
    double bid_offset = half_spread + inventory_skew + alpha_skew + pair_skew;
    double ask_offset = half_spread - inventory_skew - alpha_skew - pair_skew;

    // Ensure minimum spread of 0.1 pip
    bid_offset = std::max(bid_offset, 0.05 * pip);
    ask_offset = std::max(ask_offset, 0.05 * pip);

    Price bid = input.fair_value_mid - bid_offset;
    Price ask = input.fair_value_mid + ask_offset;

    double spread_bps = (ask - bid) / input.fair_value_mid * 10000.0;

    return SpreadOutput{
        .bid = bid,
        .ask = ask,
        .spread_bps = spread_bps,
        .half_spread = (ask - bid) / 2.0,
    };
}

} // namespace efx
