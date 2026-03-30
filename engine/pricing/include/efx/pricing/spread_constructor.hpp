#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/pricing/volatility.hpp"

#include <string>
#include <unordered_map>

namespace efx {

// Constructs final bid/ask prices per client per pair using:
//   base spread -> vol adjustment -> inventory skew -> tier multiplier -> alpha skew -> override
class SpreadConstructor {
public:
    explicit SpreadConstructor(const AppConfig& config);

    struct SpreadInput {
        std::string pair;
        double fair_value_mid;
        double volatility;
        double position;           // current inventory in base currency
        TierId tier;
        double client_alpha;       // client's alpha score
        double pair_alpha;         // directional signal on the pair
        double session_multiplier; // current session spread multiplier
        double override_pct;       // client+pair specific override (e.g., -10% = tighter)
    };

    struct SpreadOutput {
        Price bid;
        Price ask;
        double spread_bps;
        double half_spread;
    };

    SpreadOutput compute(const SpreadInput& input) const;

private:
    const AppConfig& config_;
};

} // namespace efx
