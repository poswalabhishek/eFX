#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"

#include <unordered_map>
#include <string>
#include <chrono>
#include <optional>

namespace efx {

// Maintains the latest quote from each venue for a given pair.
struct VenueQuote {
    Price bid;
    Price ask;
    Amount bid_size;
    Amount ask_size;
    Timestamp last_update;
    bool is_stale;
};

// Tracks the order book state across all venues for all pairs.
class VenueBookManager {
public:
    explicit VenueBookManager(const AppConfig& config);

    void on_tick(const VenueTick& tick);

    // Mark stale venues based on time threshold
    void check_staleness(Timestamp now);

    struct AggregatedQuote {
        double weighted_mid;
        double total_weight;
        int active_venue_count;
        Price best_bid;
        Price best_ask;
    };

    // Get weighted mid-price and venue count for a pair
    std::optional<AggregatedQuote> get_aggregated(const std::string& pair) const;

    // Get a specific venue's quote for a pair
    const VenueQuote* get_venue_quote(const std::string& pair, const std::string& venue) const;

    // Get all venue quotes for a pair
    const std::unordered_map<std::string, VenueQuote>*
    get_pair_quotes(const std::string& pair) const;

private:
    double compute_weight(const std::string& venue_id, const VenueQuote& quote,
                          Timestamp now) const;

    const AppConfig& config_;

    // pair -> (venue -> quote)
    std::unordered_map<std::string, std::unordered_map<std::string, VenueQuote>> books_;
};

} // namespace efx
