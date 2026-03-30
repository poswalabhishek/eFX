# EFX — Electronic FX Market-Making Platform

## Executive Summary

EFX is a full-stack electronic foreign exchange market-making platform built from scratch. It simulates the complete lifecycle of an eFX desk: sourcing market data from primary and secondary venues, constructing fair-value pricing, applying client-specific tiered spreads with signal-based adjustments, distributing prices across multiple platforms and a proprietary single-dealer platform (SDP), executing trades, managing risk via auto-hedging, and providing real-time analytics with alerting.

The system is designed for realistic simulation with an architecture that could transition to live market connectivity.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Technology Stack](#2-technology-stack)
3. [Module Breakdown](#3-module-breakdown)
4. [Pricing Engine (C++)](#4-pricing-engine-c)
5. [Market Data Simulator](#5-market-data-simulator)
6. [Fair Value Model](#6-fair-value-model)
7. [Spread & Tier System](#7-spread--tier-system)
8. [Client Alpha & Signal Model](#8-client-alpha--signal-model)
9. [Order Management & Execution](#9-order-management--execution)
10. [Auto-Hedger](#10-auto-hedger)
11. [Risk Management](#11-risk-management)
12. [Analytics & PnL](#12-analytics--pnl)
13. [Distribution Layer](#13-distribution-layer)
14. [Single Dealer Platform (SDP)](#14-single-dealer-platform-sdp)
15. [Internal Dashboard](#15-internal-dashboard)
16. [Data Layer](#16-data-layer)
17. [Session Management (24h Rotation)](#17-session-management-24h-rotation)
18. [Alerting System](#18-alerting-system)
19. [Client Simulation Engine](#19-client-simulation-engine)
20. [Configuration System](#20-configuration-system)
21. [Project Structure](#21-project-structure)
22. [Build Phases](#22-build-phases)
23. [Open Decisions](#23-open-decisions)

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           APEX FX PLATFORM                                  │
│                                                                             │
│  ┌───────────────┐     ┌──────────────────────────────────┐                 │
│  │ MARKET DATA   │     │        PRICING ENGINE (C++)        │                │
│  │ SIMULATOR     │     │                                    │                │
│  │               │     │  ┌────────────┐  ┌─────────────┐  │                │
│  │ EBS ─────────────▶  │  Fair Value  │─▶│ Spread      │  │                │
│  │ Reuters ─────────▶  │  Calculator  │  │ Constructor │  │                │
│  │ Hotspot ─────────▶  │  (VWAP+EWMA) │  │             │  │                │
│  │ Currenex ────────▶  │              │  │ • Base      │  │                │
│  │               │     │  ┌────────────┘  │ • Tier adj  │  │                │
│  │ Characteristics│     │  │ Volatility   │ • Alpha skew│  │                │
│  │ • Tick rate   │     │  │ Estimator    │ • Vol adj   │  │                │
│  │ • Spread width│     │  └──────────────┘ └──────┬──────┘  │                │
│  │ • Staleness   │     │                          │         │                │
│  │ • Reliability │     └──────────────────────────┼─────────┘                │
│  └───────────────┘                                │                          │
│                                                   ▼                          │
│                                     ┌──────────────────────┐                 │
│                                     │  DISTRIBUTION LAYER  │                 │
│                                     │                      │                 │
│                                     │  EBS    Reuters      │                 │
│                                     │  Hotspot  Currenex   │                 │
│                                     │  SDP (Web)           │                 │
│                                     └──────────┬───────────┘                 │
│                                                │                             │
│                           ┌────────────────────┼────────────────────┐        │
│                           ▼                    ▼                    ▼        │
│                    ┌─────────────┐    ┌──────────────┐    ┌──────────────┐   │
│                    │   CLIENT    │    │   CLIENT     │    │   CLIENT     │   │
│                    │  SIMULATOR  │    │  SIMULATOR   │    │  SIMULATOR   │   │
│                    │  (toxic)    │    │  (benign)    │    │  (large)     │   │
│                    └──────┬──────┘    └──────┬───────┘    └──────┬───────┘   │
│                           │                  │                   │           │
│                           └──────────────────┼───────────────────┘           │
│                                              ▼                               │
│                                   ┌───────────────────┐                      │
│                                   │  ORDER MANAGEMENT  │                      │
│                                   │  Fill ─▶ Position  │                      │
│                                   └────────┬──────────┘                      │
│                                            │                                 │
│                          ┌─────────────────┼─────────────────┐               │
│                          ▼                                   ▼               │
│                ┌──────────────────┐              ┌───────────────────┐       │
│                │   AUTO-HEDGER    │              │    ANALYTICS      │       │
│                │                  │              │                   │       │
│                │  • Immediate     │              │  • Real-time PnL  │       │
│                │  • Smart routing │              │  • Drawdowns      │       │
│                │  • Venue select  │              │  • Client alpha   │       │
│                └──────────────────┘              │  • Volume/ranking │       │
│                                                  │  • Alerts         │       │
│                                                  └───────────────────┘       │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                         DATA LAYER                                     │  │
│  │   QuestDB (tick data, trades, PnL)    PostgreSQL (config, clients)    │  │
│  │   Redis (real-time state, pub/sub)    ZeroMQ (engine ↔ analytics)     │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Technology Stack

| Component             | Technology                                        | Rationale                                                                       |
| --------------------- | ------------------------------------------------- | ------------------------------------------------------------------------------- |
| Pricing Engine        | **C++20**                                         | Industry-standard for eFX, microsecond tick processing, direct hardware control |
| Auto-Hedger           | **C++20**                                         | Co-located with engine for minimal latency on hedge decisions                   |
| Market Data Simulator | **C++20**                                         | High-frequency tick generation must match engine throughput                     |
| Order Management      | **C++20**                                         | Fill latency matters; shared memory with pricing engine                         |
| API Gateway           | **Python (FastAPI)**                              | Bridges C++ engine to frontend; REST + WebSocket                                |
| Analytics & Alpha     | **Python**                                        | NumPy/Pandas for PnL, scikit-learn/XGBoost for alpha model                      |
| Tick Database         | **QuestDB**                                       | Column-oriented, SQL-compatible, millions of inserts/sec, time-series native    |
| Config Database       | **PostgreSQL**                                    | Relational data: clients, tiers, overrides, sessions                            |
| Real-time State       | **Redis**                                         | Pub/sub for alerts, caching positions, sharing state                            |
| IPC (hot path)        | **ZeroMQ**                                        | Engine → Analytics pub/sub, low-latency, no broker overhead                     |
| Frontend (SDP)        | **React + TypeScript + Vite**                     | Modern, fast, WebSocket-native for streaming prices                             |
| Frontend Charts       | **TradingView Lightweight Charts** or **D3.js**   | Professional financial charting                                                 |
| WebSocket             | **Native (C++: Boost.Beast, Python: websockets)** | Streaming prices to frontend                                                    |

### C++ Libraries

| Library                       | Purpose                               | Notes                                       |
| ----------------------------- | ------------------------------------- | ------------------------------------------- |
| **Boost.Asio**                | Async I/O, networking, timers         | De facto standard for C++ async networking  |
| **Boost.Beast**               | HTTP/WebSocket built on Asio          | Engine-to-gateway WebSocket streaming       |
| **ZeroMQ (cppzmq)**           | IPC between engine and Python         | Header-only C++ binding                     |
| **simdjson**                  | JSON parsing                          | SIMD-accelerated, ~2 GB/s throughput        |
| **nlohmann/json**             | JSON serialization (config, messages) | Developer-friendly, used for non-hot-path   |
| **toml++**                    | TOML config parsing                   | Header-only, C++17                          |
| **spdlog**                    | Logging                               | Lock-free, nanosecond timestamps            |
| **fmt**                       | String formatting                     | Fast, type-safe (part of C++20 std::format) |
| **hiredis**                   | Redis client                          | C library with thin C++ wrapper             |
| **Catch2** or **Google Test** | Unit testing                          | Industry standard                           |
| **folly** (optional)          | Lock-free queues, MPMCQueue           | Facebook's high-perf C++ library            |

### Build System

**CMake** with the following structure:

- C++20 standard (`-std=c++20`)
- Compiler: Clang 16+ or GCC 13+ (for full C++20 coroutine support)
- Sanitizers enabled in debug builds (`-fsanitize=address,undefined`)
- Link-time optimization (`-flto`) in release builds
- vcpkg or Conan for dependency management

### Why not KDB+/q for v1?

KDB+ is the gold standard for tick databases but has licensing constraints and a steep learning curve for anyone unfamiliar with q. **QuestDB** provides 90% of the performance for this use case with standard SQL, and the architecture is designed so the DB layer can be swapped. A migration path to KDB+ is documented in [Open Decisions](#23-open-decisions).

---

## 3. Module Breakdown

```
apex-fx/
│
├── engine/                    # C++ (CMake project)
│   ├── core/                  # Shared types, timestamps, currency pairs
│   │   ├── include/apex/core/ # Public headers
│   │   └── src/
│   ├── pricing/               # Fair value calculation, spread construction
│   │   ├── include/apex/pricing/
│   │   └── src/
│   ├── market_data/           # Market data feed handler + simulator
│   │   ├── include/apex/market_data/
│   │   └── src/
│   ├── order/                 # Order management, fill engine
│   │   ├── include/apex/order/
│   │   └── src/
│   ├── hedger/                # Auto-hedging strategies
│   │   ├── include/apex/hedger/
│   │   └── src/
│   ├── risk/                  # Position management, risk limits
│   │   ├── include/apex/risk/
│   │   └── src/
│   ├── gateway/               # WebSocket/ZMQ bridge to outside world
│   │   ├── include/apex/gateway/
│   │   └── src/
│   ├── app/                   # main() entry points
│   ├── tests/                 # Unit and integration tests
│   └── CMakeLists.txt         # Top-level CMake
│
├── analytics/                 # Python package
│   ├── pnl/                   # Real-time and historical PnL
│   ├── alpha/                 # Client alpha model (training + inference)
│   ├── drawdown/              # Drawdown detection and alerting
│   ├── volume/                # Volume tracking and venue ranking
│   └── notebooks/             # Jupyter notebooks for ad-hoc analysis
│
├── gateway/                   # Python FastAPI service
│   ├── api/                   # REST endpoints
│   ├── ws/                    # WebSocket handlers (price streaming, blotter)
│   └── bridge/                # ZMQ subscriber, connects to C++ engine
│
├── frontend/                  # React + TypeScript
│   ├── src/
│   │   ├── components/
│   │   │   ├── sdp/           # Client-facing SDP components
│   │   │   ├── dashboard/     # Internal desk dashboard
│   │   │   └── shared/        # Common UI components
│   │   ├── hooks/             # WebSocket hooks, data hooks
│   │   ├── stores/            # State management (Zustand)
│   │   └── types/             # TypeScript type definitions
│   └── public/
│
├── config/                    # Configuration files
│   ├── tiers.toml             # Tier definitions and base spreads
│   ├── clients.toml           # Client configurations
│   ├── venues.toml            # Venue/platform configurations
│   ├── pairs.toml             # Currency pair configurations
│   └── sessions.toml          # Trading session definitions
│
├── simulator/                 # Python — client flow simulation
│   ├── client_profiles.py     # Toxic, benign, large, small archetypes
│   ├── flow_generator.py      # Random + patterned trade generation
│   └── scenario_runner.py     # Run simulation scenarios
│
├── migrations/                # Database migrations
│   ├── questdb/
│   └── postgres/
│
├── docker-compose.yml         # Full stack orchestration
├── CMakeLists.txt             # Root CMake (delegates to engine/)
├── vcpkg.json                 # C++ dependency manifest
├── pyproject.toml             # Python project config
└── PLAN.md                    # This file
```

---

## 4. Pricing Engine (C++)

The pricing engine is the core of the platform. It runs as a single C++ process with a multi-threaded architecture using Boost.Asio for async I/O and a dedicated thread pool for compute.

### Responsibilities

1. Ingest market data ticks from all venues (simulated or real)
2. Maintain an order book snapshot per venue per pair
3. Calculate fair value mid using VWAP + power-smoothed EWMA
4. Estimate real-time volatility
5. Construct outgoing bid/ask prices per client tier
6. Publish prices to distribution layer
7. Accept incoming orders, execute fills
8. Forward fills to hedger and analytics

### Internal Architecture

```
                    ┌─────────────────────────────────┐
                    │        Pricing Engine            │
                    │                                  │
 Market Data ──────▶│  ┌───────────────────────┐      │
 (per venue)        │  │  Venue Book Manager   │      │
                    │  │  • Best bid/ask per LP │      │
                    │  │  • Staleness detection │      │
                    │  │  • Venue weighting     │      │
                    │  └───────────┬───────────┘      │
                    │              │                    │
                    │              ▼                    │
                    │  ┌───────────────────────┐      │
                    │  │  Fair Value Engine     │      │
                    │  │  • Volume-weighted mid │      │
                    │  │  • Power-smoothed EWMA │      │
                    │  │  • Outlier rejection   │      │
                    │  └───────────┬───────────┘      │
                    │              │                    │
                    │              ▼                    │
                    │  ┌───────────────────────┐      │
                    │  │  Volatility Estimator │      │
                    │  │  • Realized vol (tick) │      │
                    │  │  • Vol regime detect   │      │
                    │  └───────────┬───────────┘      │
                    │              │                    │
                    │              ▼                    │
                    │  ┌───────────────────────┐      │──────▶ Price Stream
                    │  │  Spread Constructor   │      │        (per client
                    │  │  • Base spread         │      │         per pair)
                    │  │  • Tier adjustment     │      │
                    │  │  • Alpha/signal skew   │      │
                    │  │  • Volatility widening │      │
                    │  │  • Inventory skew      │      │
                    │  └───────────────────────┘      │
                    └─────────────────────────────────┘
```

### Key Data Structures

```cpp
struct VenueTick {
    VenueId venue;
    CurrencyPair pair;
    Price bid;
    Price ask;
    Amount bid_size;
    Amount ask_size;
    Timestamp timestamp;   // nanosecond precision (std::chrono::steady_clock)
};

struct FairValue {
    CurrencyPair pair;
    Price mid;
    double confidence;     // 0.0–1.0 based on venue agreement
    double volatility;     // annualized, real-time estimated
    Timestamp timestamp;
};

struct ClientPrice {
    ClientId client_id;
    CurrencyPair pair;
    Price bid;
    Price ask;
    double spread_bps;
    uint32_t valid_for_ms; // quote validity window
    QuoteId quote_id;
    Timestamp timestamp;
};
```

### Threading Model

```
┌─────────────────────────────────────────────────────┐
│                    Main Process                       │
│                                                       │
│  Thread 1: I/O Thread (Boost.Asio io_context)        │
│    • Market data ingestion (ZMQ SUB sockets)         │
│    • WebSocket connections (Boost.Beast)              │
│    • Timer-based price publishing                     │
│                                                       │
│  Thread 2: Pricing Thread (pinned to core)            │
│    • Fair value calculation                           │
│    • Spread construction                              │
│    • Lock-free queue from I/O thread                  │
│                                                       │
│  Thread 3: Order Thread                               │
│    • Fill execution                                   │
│    • Position updates                                 │
│    • Hedge trigger                                    │
│                                                       │
│  Thread 4: Analytics Publisher                        │
│    • ZMQ PUB to Python analytics                      │
│    • QuestDB line protocol writer                     │
│                                                       │
│  Communication: SPSC lock-free ring buffers between   │
│  threads (no mutexes on the hot path)                 │
└─────────────────────────────────────────────────────┘
```

### Throughput Targets

| Metric                          | Target                                     |
| ------------------------------- | ------------------------------------------ |
| Tick ingestion                  | 100,000+ ticks/sec across all venues/pairs |
| Fair value update               | < 10 μs per tick                           |
| Price construction (per client) | < 5 μs                                     |
| End-to-end tick-to-quote        | < 50 μs                                    |
| Concurrent client price streams | 1,000+                                     |

---

## 5. Market Data Simulator

Generates realistic FX market data for 4 venues across all configured currency pairs.

### Venue Characteristics

| Venue        | Type      | Avg Tick Rate         | Spread                      | Behavior                                                 |
| ------------ | --------- | --------------------- | --------------------------- | -------------------------------------------------------- |
| **EBS**      | Primary   | 50–200/sec for EURUSD | Ultra-tight (0.1–0.3 pip)   | Gold standard for EUR, JPY pairs. Goes wide during news. |
| **Reuters**  | Primary   | 30–150/sec for GBPUSD | Tight (0.2–0.5 pip)         | Dominant for GBP, commodity currencies.                  |
| **Hotspot**  | Secondary | 20–80/sec             | Moderate (0.3–0.8 pip)      | More retail/institutional flow, steadier.                |
| **Currenex** | Secondary | 15–60/sec             | Moderate–Wide (0.5–1.2 pip) | Corporate/real money flow, less volatile.                |

### Simulation Features

- **Correlated prices**: All venues track a shared "true" mid-price with venue-specific noise and latency
- **Microstructure**: Bid-ask bounce, mean-reversion at short timescales
- **Regime switching**: Normal → volatile → illiquid transitions (random or scheduled around "news events")
- **Staleness**: Venues randomly go stale (stop updating) for 100ms–5s, especially secondary venues
- **Time-of-day patterns**: Spread tightens during London/NY overlap, widens during Tokyo
- **Correlation structure**: Cross-pair correlations (EURUSD and GBPUSD move together, etc.)

### True Mid-Price Process

The underlying "true" price for each pair follows a jump-diffusion process:

```
dS = μ·dt + σ·dW + J·dN

where:
  μ  = drift (near zero for spot FX over short horizons)
  σ  = volatility (varies by pair and session)
  dW = Wiener process (Brownian motion)
  J  = jump size (drawn from normal distribution)
  dN = Poisson process (rare large moves, ~2-5 per day)
```

Each venue then observes this true price with:

- A latency offset (1–10ms depending on venue)
- An additive noise term (venue-specific spread)
- A staleness probability

---

## 6. Fair Value Model

### Step 1: Volume-Weighted Aggregation

For each currency pair, at each moment we have up to 4 venue quotes. The raw mid is computed as a volume-weighted average:

```
mid_raw = Σ(w_i · mid_i) / Σ(w_i)

where w_i = f(venue_volume_i, venue_reliability_i, freshness_i)
```

Weights incorporate:

- **Volume**: Higher-volume venues contribute more
- **Freshness**: Stale quotes (> configurable threshold, e.g., 500ms) get exponentially decaying weight
- **Venue priority**: Primary venues (EBS, Reuters) get a base weight multiplier over secondary

### Step 2: Power-Smoothed EWMA

The raw mid is then smoothed using an exponentially weighted moving average with a power transformation to reduce the impact of outlier ticks:

```
fair_value_t = α · sign(mid_raw_t) · |mid_raw_t|^p + (1 - α) · fair_value_{t-1}

where:
  α = smoothing factor (configurable, ~0.3 for fast response, ~0.05 for stable)
  p = power parameter (< 1.0 compresses outliers, e.g., 0.85)
```

In practice, the power transform is applied to the **delta** (change from previous value), not the absolute price:

```
delta_t = mid_raw_t - fair_value_{t-1}
smoothed_delta = sign(delta_t) · |delta_t|^p
fair_value_t = fair_value_{t-1} + α · smoothed_delta
```

### Step 3: Confidence Score

A confidence score (0–1) is computed based on:

- Number of active (non-stale) venues contributing
- Agreement between venues (low dispersion = high confidence)
- Recency of last tick

Low confidence triggers wider spreads automatically.

---

## 7. Spread & Tier System

### Spread Construction Pipeline

```
Fair Value Mid
      │
      ▼
┌─────────────┐
│ Base Spread  │  ← per-pair default (e.g., EURUSD = 0.2 pip, USDJPY = 0.3 pip)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Volatility  │  ← widens spread proportional to realized vol
│ Adjustment  │    spread_vol = base × (1 + k × vol / vol_baseline)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Inventory   │  ← skews bid/ask based on current position
│ Skew        │    if long: lower bid more, keep ask (encourage sells)
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│ Client Tier     │  ← multiplier based on client's assigned tier
│ Adjustment      │    spread_client = spread × tier_multiplier
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│ Client Alpha    │  ← skew based on client's historical signal
│ Skew            │    toxic clients get asymmetric widening
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│ Override Layer  │  ← client-specific, pair-specific adjustments
│                 │    e.g., ClientX + EURUSD = Tier 1 - 10%
└──────┬──────────┘
       │
       ▼
  Final Bid/Ask
```

### Tier Definitions

5 base tiers with configurable multipliers:

| Tier | Name       | Spread Multiplier | Typical Client                     |
| ---- | ---------- | ----------------- | ---------------------------------- |
| 1    | Platinum   | 1.0×              | High-value, low-alpha, high-volume |
| 2    | Gold       | 1.5×              | Good clients, moderate volume      |
| 3    | Silver     | 2.5×              | Standard clients                   |
| 4    | Bronze     | 4.0×              | Low volume or mildly toxic         |
| 5    | Restricted | 8.0×              | Highly toxic, under review         |

### Override System (Priority Order, highest wins)

1. **Client + Pair override**: `ClientX on EURUSD → Tier 1 with -10% adjustment`
2. **Client override**: `ClientX → Tier 2 globally`
3. **Pair default**: `EURUSD → base spread 0.2 pip`
4. **Global default**: `all pairs → Tier 3`

### Override Configuration Example

```toml
# config/tiers.toml

[tiers]
platinum = { multiplier = 1.0, label = "Platinum" }
gold     = { multiplier = 1.5, label = "Gold" }
silver   = { multiplier = 2.5, label = "Silver" }
bronze   = { multiplier = 4.0, label = "Bronze" }
restricted = { multiplier = 8.0, label = "Restricted" }

[base_spreads]
EURUSD = 0.2   # pips
USDJPY = 0.3
GBPUSD = 0.3
AUDUSD = 0.4
USDCHF = 0.4
USDCAD = 0.4
NZDUSD = 0.5
EURGBP = 0.4
EURJPY = 0.5
GBPJPY = 0.6

# Client-specific overrides
[[client_overrides]]
client_id = "hedge_fund_alpha"
default_tier = "gold"

[[client_overrides.pair_overrides]]
pair = "EURUSD"
tier = "platinum"
adjustment_pct = -10.0   # 10% tighter than platinum

[[client_overrides.pair_overrides]]
pair = "USDJPY"
tier = "silver"
adjustment_pct = 0.0

[[client_overrides]]
client_id = "toxic_hft"
default_tier = "restricted"
```

---

## 8. Client Alpha & Signal Model

### What is Client Alpha?

Alpha measures how much a client's trades predict future price movement. A client with **positive alpha** (from their perspective) is "toxic" — they consistently trade in the direction the market is about to move, meaning the market-maker loses money after filling them.

### Measurement

For each client trade, mark-to-market the fill against the mid-price at multiple horizons:

```
alpha(t, horizon) = direction × (mid_{t+horizon} - mid_t) / mid_t

where:
  direction = +1 for client buy, -1 for client sell
  horizon ∈ {1s, 5s, 10s, 30s, 60s, 300s}
```

Positive alpha = client is informed (toxic).
Negative alpha = client is a liquidity provider to us (benign).

### Model Architecture

A lightweight model is retrained daily on the previous N days of trade data:

**Features per client:**

- Rolling alpha at each horizon (1s, 5s, 30s, 60s)
- Trade frequency and size distribution
- Time-of-day patterns
- Pair concentration
- Fill rate (how often they trade vs. just look)
- Adverse selection ratio (% of trades where price moved against us within 1s)

**Output:**

- Alpha score: continuous value, mapped to a spread adjustment
- Toxicity classification: low / medium / high / extreme
- Recommended tier (advisory, human can override)

**Training pipeline:**

1. Every day at a configurable time (e.g., 22:00 UTC), pull last 30 days of trade data
2. Compute features per client
3. Train gradient-boosted model (XGBoost) to predict forward alpha
4. Export model coefficients to Redis for real-time inference
5. Pricing engine reads coefficients and applies in spread construction

### Real-Time Application

During trading, for each incoming client order, the engine:

1. Looks up client's latest alpha score (from daily model)
2. Adjusts the spread asymmetrically:
   - Toxic client buying → widen the ask more (they're likely right about direction)
   - Toxic client selling → widen the bid more
3. The adjustment magnitude is proportional to the alpha score

---

## 9. Order Management & Execution

### Order Types (Phase 1)

- **Market Order**: Client hits displayed bid or lifts displayed ask, fills immediately at displayed price
- **Limit Order**: Client specifies a price; fills if market reaches it (Phase 2)

### Execution Flow

```
1. Client submits order (via SDP or simulated platform)
       │
       ▼
2. Order validation
   • Is the quote still valid? (check quote_id, timestamp)
   • Does client have credit? (simplified for simulation)
   • Is the pair tradeable in current session?
       │
       ▼
3. Fill execution
   • Match at the quoted price
   • Generate fill record with nanosecond timestamp
   • Assign unique trade_id
       │
       ▼
4. Post-trade
   ├──▶ Update position (Risk Manager)
   ├──▶ Trigger auto-hedger
   ├──▶ Publish to analytics (PnL, alpha)
   ├──▶ Publish to client (fill confirmation)
   └──▶ Write to database (trade record)
```

### Fill Record

```cpp
struct Fill {
    TradeId trade_id;
    QuoteId quote_id;
    ClientId client_id;
    CurrencyPair pair;
    Side side;             // Buy or Sell (from client's perspective)
    Price price;
    Amount amount;
    VenueId venue;         // which platform the trade came through
    Timestamp fill_timestamp;
    Price mid_at_fill;     // fair value mid at time of fill (for alpha calc)
};
```

---

## 10. Auto-Hedger

### Strategy: Immediate Full Hedge (Phase 1)

Upon receiving a fill, the hedger immediately seeks to flatten the resulting position by trading in the simulated interbank market.

```
1. Receive fill notification (e.g., client bought 1M EURUSD at 1.08505)
       │
       ▼
2. We are now short 1M EURUSD
       │
       ▼
3. Hedger evaluates venues:
   • EBS ask: 1.08503 (5M available)
   • Reuters ask: 1.08504 (3M available)
   • Hotspot ask: 1.08505 (2M available)
       │
       ▼
4. Execute hedge: Buy 1M EURUSD on EBS at 1.08503
       │
       ▼
5. Position flattened. PnL on trade = (1.08505 - 1.08503) × 1M = $20
```

### Smart Order Routing (Phase 2)

- Split large hedges across venues for better execution
- Time-slice hedges to avoid market impact
- Skew hedge urgency based on alpha score (toxic → hedge faster)

### Hedge Record

```cpp
struct HedgeExecution {
    HedgeId hedge_id;
    TradeId triggering_trade_id;
    CurrencyPair pair;
    Side side;
    Price price;
    Amount amount;
    VenueId venue;
    double slippage_bps;   // vs. fair value at time of hedge
    uint64_t latency_us;   // time from fill to hedge execution
    Timestamp timestamp;
};
```

---

## 11. Risk Management

### Position Tracking

Real-time position per currency pair:

```
Position(EURUSD) = Σ(client_fills) + Σ(hedge_fills)

where buys are positive, sells are negative
```

### Risk Limits

| Limit                     | Description                                 | Action                              |
| ------------------------- | ------------------------------------------- | ----------------------------------- |
| Max position per pair     | e.g., ±50M EURUSD                           | Reject new trades that would breach |
| Max total exposure        | Aggregate across all pairs (USD equivalent) | Widen all spreads, alert            |
| Max drawdown (daily)      | e.g., -$500K                                | Alert + widen spreads + escalate    |
| Max drawdown (rolling 1h) | e.g., -$100K                                | Alert + auto-widen spreads          |
| Hedge failure             | Hedge not executed within 100ms             | Alert + pause pricing on that pair  |

### Inventory Skew

When position builds up, the pricing engine skews prices to encourage offsetting flow:

```
if position > 0 (long):
    bid_skew  = -skew_factor × position / max_position  (lower bid, less buying)
    ask_skew  = -skew_factor × position / max_position × 0.5 (slightly lower ask, attract sellers)

if position < 0 (short):
    mirror of above
```

---

## 12. Analytics & PnL

### Real-Time PnL

PnL is computed in real-time with multiple components:

```
Total PnL = Spread PnL + Position PnL + Hedge Cost

where:
  Spread PnL    = Σ(half-spread captured on each client fill)
  Position PnL  = mark-to-market of open positions vs. current fair value
  Hedge Cost    = Σ(slippage incurred on hedge executions)
```

### PnL Dimensions

PnL is tracked and can be sliced by:

- **Time**: Real-time, hourly, daily, weekly, MTD
- **Currency pair**: Per pair PnL attribution
- **Client**: Which clients are profitable/unprofitable
- **Venue/Platform**: PnL per distribution channel
- **Session**: Tokyo / London / New York
- **Component**: Spread capture vs. position PnL vs. hedge cost

### Drawdown Detection

Continuously monitors PnL for drawdowns:

```python
class DrawdownDetector:
    """
    Tracks running max PnL and alerts when drawdown exceeds thresholds.

    Thresholds:
      - Warning:  -$50K from peak (dashboard yellow)
      - Critical: -$100K from peak (dashboard red + audible)
      - Emergency: -$250K from peak (auto-widen all spreads)
    """
```

### Volume & Ranking Analytics

Track market share and ranking on each platform:

```
Volume(venue, pair, period) = Σ(trade amounts)
Market Share = our_volume / total_platform_volume (simulated)
Ranking = ordinal position vs. other simulated market-makers
```

---

## 13. Distribution Layer

The distribution layer manages outgoing prices to each venue/platform. Each platform has different characteristics:

| Platform     | Price Update Rate   | Spread Strategy      | Notes                             |
| ------------ | ------------------- | -------------------- | --------------------------------- |
| **EBS**      | Every tick (~5ms)   | Tightest possible    | Primary venue, reputation matters |
| **Reuters**  | Every tick (~5ms)   | Tight                | Important for GBP, AUD            |
| **Hotspot**  | 50–100ms            | Moderate             | Cost-effective, less aggressive   |
| **Currenex** | 100–200ms           | Wider                | Corporate flow, less toxic        |
| **SDP**      | Real-time streaming | Client-tier specific | Our platform, full control        |

Each platform adapter handles:

- Rate limiting (don't exceed platform's max update rate)
- Price format conversion
- Quote ID generation and tracking
- Fill notifications back to order manager

---

## 14. Single Dealer Platform (SDP)

### Client-Facing View

```
┌─────────────────────────────────────────────────────────┐
│  APEX FX                                    [Session: LDN] │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │   EUR/USD    │  │   USD/JPY    │  │   GBP/USD    │   │
│  │              │  │              │  │              │   │
│  │  1.0850  52  │  │  149.50  55  │  │  1.2730  35  │   │
│  │  BID    ASK  │  │  BID    ASK  │  │  BID    ASK  │   │
│  │   [SELL|BUY] │  │   [SELL|BUY] │  │   [SELL|BUY] │   │
│  │  Amt: [1M ▼] │  │  Amt: [1M ▼] │  │  Amt: [1M ▼] │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │   AUD/USD    │  │   USD/CHF    │  │   USD/CAD    │   │
│  │  0.6520  25  │  │  0.8835  40  │  │  1.3620  25  │   │
│  │  BID    ASK  │  │  BID    ASK  │  │  BID    ASK  │   │
│  │   [SELL|BUY] │  │   [SELL|BUY] │  │   [SELL|BUY] │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                           │
├─────────────────────────────────────────────────────────┤
│  BLOTTER                                                  │
│  Time       Pair     Side  Amount   Price    Status       │
│  14:23:05   EURUSD   BUY   2M     1.08502   FILLED       │
│  14:22:51   USDJPY   SELL  5M     149.535    FILLED       │
│  14:22:30   GBPUSD   BUY   1M     1.27305   FILLED       │
└─────────────────────────────────────────────────────────┘
```

### Features

- **Streaming price tiles**: Real-time bid/ask via WebSocket, color-coded tick direction
- **Click-to-trade**: Single click on bid (sell) or ask (buy) with preset amounts
- **Trade blotter**: Live table of recent trades with fill confirmations
- **Session indicator**: Shows current trading session (Tokyo/London/NY)

---

## 15. Internal Dashboard

The internal dashboard is the nerve center for the eFX desk.

### Layout

```
┌────────────────────────────────────────────────────────────────────┐
│  APEX FX — DESK DASHBOARD                          [LDN Session]   │
├───────────────────────────────┬────────────────────────────────────┤
│  REAL-TIME PnL                │  POSITIONS                         │
│  ┌───────────────────────┐   │  Pair      Position    Unrealized  │
│  │    📈 +$125,400       │   │  EURUSD   +2.5M       +$1,200     │
│  │    (chart: today)     │   │  USDJPY   -5.0M       -$3,400     │
│  │                       │   │  GBPUSD   +1.0M       +$500       │
│  │  Peak: +$142,000      │   │  ...                               │
│  │  Drawdown: -$16,600   │   │  Net USD: -$45,000                 │
│  └───────────────────────┘   │                                     │
├───────────────────────────────┼────────────────────────────────────┤
│  CLIENT ALPHA HEATMAP         │  VENUE PERFORMANCE                 │
│                               │                                    │
│  Client        Alpha  Tier    │  Venue     Volume    Rank  PnL    │
│  HedgeFundA    +3.2   🔴 5   │  EBS       $2.1B     #3    +$45K  │
│  RealMoney1    -1.1   🟢 1   │  Reuters   $1.8B     #5    +$38K  │
│  CorpTreasury  -0.5   🟢 2   │  Hotspot   $0.9B     #7    +$22K  │
│  AlgoFirm3     +5.8   🔴 5   │  Currenex  $0.4B     #12   +$12K  │
│  RetailAgg     +0.2   🟡 3   │  SDP       $0.6B     —     +$18K  │
│                               │                                    │
├───────────────────────────────┴────────────────────────────────────┤
│  ALERTS                                                             │
│  ⚠  14:15 — USDJPY drawdown -$8,200 in last 30min                 │
│  🔴 14:10 — AlgoFirm3 alpha spike: +5.8 (was +2.1 yesterday)      │
│  ℹ  14:00 — Session handoff: Tokyo → London complete               │
└────────────────────────────────────────────────────────────────────┘
```

### Features

- **PnL waterfall chart**: Real-time with drawdown overlay
- **Position grid**: Per-pair with unrealized PnL, heatmapped
- **Client alpha heatmap**: Color-coded toxicity, sortable
- **Venue ranking board**: Volume, market share, PnL per platform
- **Alert feed**: Scrolling alerts with severity levels and audible notifications
- **Tier management panel**: View/edit client tiers and overrides in real-time
- **Fair value monitor**: Shows fair value mid vs. each venue's quote, highlights divergence

---

## 16. Data Layer

### QuestDB — Tick & Trade Data

**Tables:**

```sql
-- Raw market data ticks from all venues
CREATE TABLE market_ticks (
    timestamp TIMESTAMP,
    venue     SYMBOL,
    pair      SYMBOL,
    bid       DOUBLE,
    ask       DOUBLE,
    bid_size  DOUBLE,
    ask_size  DOUBLE
) TIMESTAMP(timestamp) PARTITION BY HOUR;

-- Fair value snapshots
CREATE TABLE fair_values (
    timestamp   TIMESTAMP,
    pair        SYMBOL,
    mid         DOUBLE,
    confidence  DOUBLE,
    volatility  DOUBLE
) TIMESTAMP(timestamp) PARTITION BY HOUR;

-- Client fills
CREATE TABLE fills (
    timestamp     TIMESTAMP,
    trade_id      SYMBOL,
    client_id     SYMBOL,
    pair          SYMBOL,
    side          SYMBOL,
    price         DOUBLE,
    amount        DOUBLE,
    venue         SYMBOL,
    mid_at_fill   DOUBLE,
    spread_bps    DOUBLE
) TIMESTAMP(timestamp) PARTITION BY DAY;

-- Hedge executions
CREATE TABLE hedges (
    timestamp          TIMESTAMP,
    hedge_id           SYMBOL,
    triggering_trade   SYMBOL,
    pair               SYMBOL,
    side               SYMBOL,
    price              DOUBLE,
    amount             DOUBLE,
    venue              SYMBOL,
    slippage_bps       DOUBLE,
    latency_us         LONG
) TIMESTAMP(timestamp) PARTITION BY DAY;

-- PnL snapshots (sampled every second)
CREATE TABLE pnl_snapshots (
    timestamp      TIMESTAMP,
    pair           SYMBOL,
    spread_pnl     DOUBLE,
    position_pnl   DOUBLE,
    hedge_cost     DOUBLE,
    total_pnl      DOUBLE,
    position       DOUBLE,
    drawdown       DOUBLE
) TIMESTAMP(timestamp) PARTITION BY DAY;
```

### PostgreSQL — Configuration & State

```sql
-- Client master data
CREATE TABLE clients (
    client_id     VARCHAR PRIMARY KEY,
    name          VARCHAR NOT NULL,
    default_tier  VARCHAR NOT NULL DEFAULT 'silver',
    alpha_score   DOUBLE PRECISION DEFAULT 0.0,
    is_active     BOOLEAN DEFAULT true,
    created_at    TIMESTAMPTZ DEFAULT NOW()
);

-- Client-pair overrides
CREATE TABLE client_pair_overrides (
    client_id      VARCHAR REFERENCES clients(client_id),
    pair           VARCHAR NOT NULL,
    tier_override  VARCHAR,
    adjustment_pct DOUBLE PRECISION DEFAULT 0.0,
    PRIMARY KEY (client_id, pair)
);

-- Tier definitions
CREATE TABLE tiers (
    tier_id       VARCHAR PRIMARY KEY,
    label         VARCHAR NOT NULL,
    multiplier    DOUBLE PRECISION NOT NULL,
    sort_order    INTEGER NOT NULL
);

-- Trading sessions
CREATE TABLE sessions (
    session_id    VARCHAR PRIMARY KEY,
    label         VARCHAR NOT NULL,
    start_utc     TIME NOT NULL,
    end_utc       TIME NOT NULL,
    config_json   JSONB
);
```

### Redis — Real-Time State

```
# Current positions (hash)
position:EURUSD → { amount: 2500000, avg_price: 1.08503, timestamp: ... }

# Latest fair values (hash, updated every tick)
fair_value:EURUSD → { mid: 1.08504, confidence: 0.95, vol: 0.065 }

# Client alpha scores (hash, updated daily)
alpha:hedge_fund_alpha → { score: 3.2, toxicity: "high", updated: "2026-03-30T22:00:00Z" }

# Alert channel (pub/sub)
PUBLISH alerts '{"severity":"warning","message":"USDJPY drawdown -$8200","timestamp":"..."}'

# Real-time PnL (sorted set for time-series)
ZADD pnl:today <timestamp> '{"total":125400,"spread":98000,"position":32400,"hedge":-5000}'
```

### Data Retention

| Data              | Hot (QuestDB) | Warm | Cold                  |
| ----------------- | ------------- | ---- | --------------------- |
| Market ticks      | 3 days        | —    | Parquet files on disk |
| Fair values       | 7 days        | —    | Parquet files on disk |
| Fills/Hedges      | 90 days       | —    | Parquet archive       |
| PnL snapshots     | 30 days       | —    | Parquet archive       |
| Config (Postgres) | Forever       | —    | —                     |

---

## 17. Session Management (24h Rotation)

### Session Definitions

| Session      | UTC Hours   | Focus Pairs           | Spread Profile                   | Notes                     |
| ------------ | ----------- | --------------------- | -------------------------------- | ------------------------- |
| **Tokyo**    | 21:00–06:00 | JPY, AUD, NZD crosses | Wider on EUR/GBP, tighter on JPY | Lower liquidity overall   |
| **London**   | 06:00–14:00 | All G10               | Tightest spreads, highest volume | Peak liquidity window     |
| **New York** | 14:00–21:00 | USD pairs, CAD        | Moderate spreads                 | Overlap with London early |

### Session Transitions

At each session boundary:

1. Log end-of-session PnL snapshot
2. Load new session's spread configuration
3. Adjust base spreads per session profile
4. Send handoff notification to dashboard
5. Reset session-level risk counters

### Configurable Per Session

- Base spread multiplier (e.g., Tokyo = 1.5×, London = 1.0×, NY = 1.2×)
- Risk limits (tighter during low-liquidity sessions)
- Volatility thresholds for spread widening
- Active pairs (some pairs may not be quoted during certain sessions)

---

## 18. Alerting System

### Alert Types

| Alert                    | Severity  | Trigger                        | Action                       |
| ------------------------ | --------- | ------------------------------ | ---------------------------- |
| PnL drawdown (minor)     | Warning   | -$50K from peak                | Dashboard yellow banner      |
| PnL drawdown (major)     | Critical  | -$100K from peak               | Dashboard red + audible beep |
| PnL drawdown (emergency) | Emergency | -$250K from peak               | Auto-widen spreads + siren   |
| Client alpha spike       | Warning   | Alpha increases > 2σ from mean | Dashboard notification       |
| Venue staleness          | Warning   | No tick from venue > 5s        | Dashboard + log              |
| Position limit breach    | Critical  | > 80% of max position          | Dashboard red + audible      |
| Hedge failure            | Critical  | Hedge not executed in 100ms    | Dashboard red + audible      |
| Session transition       | Info      | Session boundary reached       | Dashboard notification       |

### Delivery Channels

1. **Dashboard popups**: Color-coded severity, auto-dismiss after acknowledgment
2. **Audible alerts**: Different sounds for warning vs. critical vs. emergency
3. **Alert log**: Persistent scrollable feed in dashboard

---

## 19. Client Simulation Engine

### Client Archetypes

| Archetype                | Alpha Profile       | Avg Size | Frequency         | Pairs          |
| ------------------------ | ------------------- | -------- | ----------------- | -------------- |
| **Toxic HFT**            | +3 to +8 bps at 1s  | 1–5M     | 100+ trades/day   | EURUSD, USDJPY |
| **Informed Hedge Fund**  | +1 to +3 bps at 30s | 5–25M    | 20–50 trades/day  | All G10        |
| **Real Money (Pension)** | -0.5 to +0.5 bps    | 10–100M  | 2–10 trades/day   | Majors only    |
| **Corporate Treasury**   | -1 to 0 bps         | 5–50M    | 1–5 trades/day    | Specific pairs |
| **Retail Aggregator**    | -0.5 to +0.5 bps    | 0.1–2M   | 50–200 trades/day | All pairs      |
| **Central Bank**         | Unknown pattern     | 100M+    | 0–2 trades/day    | Specific pair  |

### Simulation Behavior

Each simulated client:

1. Arrives with a configurable Poisson process (time between trades)
2. Selects a pair (weighted by their pair preference profile)
3. Selects a direction (biased by their alpha model — toxic clients trade with momentum)
4. Selects a size (drawn from their size distribution)
5. Evaluates the displayed price (may reject if spread too wide)
6. Submits order to a specific venue (weighted by their venue preference)

### Toxicity Simulation

Toxic clients peek at the true mid-price process and bias their direction accordingly:

```python
# Simplified toxic flow logic
future_move = true_mid[t + lookahead] - true_mid[t]
trade_probability = sigmoid(toxicity_factor * future_move)
# High toxicity → trades consistently in direction of future move
```

---

## 20. Configuration System

All configuration uses TOML files for human readability, loaded at startup and hot-reloadable.

### Config Files

- `config/tiers.toml` — Tier definitions, base spreads, multipliers
- `config/clients.toml` — Client list, default tiers, overrides
- `config/venues.toml` — Venue properties, update rates, primary/secondary designation
- `config/pairs.toml` — Currency pair list, pip sizes, tick sizes, base spreads
- `config/sessions.toml` — Session hours, per-session spread adjustments
- `config/risk.toml` — Position limits, drawdown thresholds, hedge parameters
- `config/simulator.toml` — Client archetypes, flow patterns, scenario definitions
- `config/alpha.toml` — Alpha model parameters, training schedule, feature config

### Hot Reload

The system watches config files for changes. When a config file is modified:

1. New config is validated
2. If valid, atomically swapped into the running system
3. Dashboard shows "Config reloaded" notification
4. If invalid, rejected with error in dashboard

---

## 21. Project Structure

(See [Module Breakdown](#3-module-breakdown) for detailed file tree)

### Build & Run

```bash
# Install C++ dependencies via vcpkg
vcpkg install --triplet x64-osx   # or x64-linux

# Build the C++ engine
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# Start infrastructure (QuestDB, PostgreSQL, Redis)
docker-compose up -d questdb postgres redis

# Run database migrations
cd migrations && ./migrate.sh

# Start the C++ pricing engine
./engine/build/bin/apex-engine --config ./config/

# Start the Python analytics service
cd analytics && python -m apex_analytics.main

# Start the Python API gateway
cd gateway && uvicorn main:app --host 0.0.0.0 --port 8000

# Start the React frontend
cd frontend && npm run dev

# Run the client simulator
cd simulator && python -m scenario_runner --profile default
```

---

## 22. Build Phases

### Phase 1: Pricing Engine Core (Weeks 1–3)

**Goal**: Market data in → fair value → spread prices out

- [ ] CMake project setup with vcpkg dependencies and `core/` library
- [ ] Core types: `CurrencyPair`, `Price`, `Timestamp`, `VenueId`, `Side`
- [ ] Market data simulator — 4 venues, realistic tick generation (jump-diffusion)
- [ ] Fair value calculator — VWAP + power-smoothed EWMA
- [ ] Volatility estimator — real-time realized vol
- [ ] Base spread construction — per pair
- [ ] Price publisher — outputs constructed prices to stdout/log
- [ ] SPSC lock-free ring buffer for inter-thread communication
- [ ] Unit tests (Google Test) for fair value convergence and spread math
- [ ] TOML config loading (toml++)

### Phase 2: Tier System & Client Pricing (Weeks 3–4)

**Goal**: Client-specific pricing with overrides

- [ ] Tier definitions and multiplier logic
- [ ] Client override system (client, pair, percentage adjustments)
- [ ] Override resolution logic (priority cascade)
- [ ] Per-client price streams
- [ ] Config hot reload for tiers
- [ ] PostgreSQL schema for clients and tiers

### Phase 3: Order Management & Execution (Weeks 4–5)

**Goal**: Clients can trade at displayed prices

- [ ] Order validation and fill engine
- [ ] Quote ID tracking and staleness checking
- [ ] Fill record generation
- [ ] Position manager — real-time position tracking
- [ ] QuestDB integration — write ticks and fills
- [ ] Basic client simulator — random flow

### Phase 4: Auto-Hedger (Weeks 5–6)

**Goal**: Positions are automatically hedged

- [ ] Immediate full-hedge strategy
- [ ] Venue selection (best price across simulated venues)
- [ ] Hedge execution recording
- [ ] Inventory skew in pricing engine
- [ ] Hedge latency and slippage simulation

### Phase 5: Analytics & PnL (Weeks 6–8)

**Goal**: Real-time PnL with drawdown detection

- [ ] Python analytics service with ZMQ subscriber
- [ ] Real-time PnL calculation (spread + position + hedge)
- [ ] PnL slicing (by pair, client, venue, session)
- [ ] Drawdown detector with configurable thresholds
- [ ] Client alpha measurement (mark-to-market at horizons)
- [ ] Volume and ranking tracking
- [ ] Jupyter notebook templates for ad-hoc analysis

### Phase 6: Alpha Model (Weeks 8–9)

**Goal**: Daily-trained model drives real-time spread adjustments

- [ ] Feature engineering pipeline (per-client features)
- [ ] XGBoost model training script
- [ ] Model export to Redis for engine consumption
- [ ] Real-time alpha skew in spread constructor
- [ ] Toxicity classification and tier recommendations
- [ ] Sophisticated client simulator (toxic/benign archetypes)

### Phase 7: API Gateway (Weeks 9–10)

**Goal**: Bridge between C++ engine and frontend

- [ ] FastAPI service with WebSocket support
- [ ] Price streaming endpoint (WS)
- [ ] Trade submission endpoint (REST + WS)
- [ ] Blotter endpoint (WS)
- [ ] PnL streaming endpoint (WS)
- [ ] Config management endpoints (REST)
- [ ] Alert streaming endpoint (WS)

### Phase 8: Frontend — SDP (Weeks 10–12)

**Goal**: Client-facing trading interface

- [ ] React + TypeScript + Vite setup
- [ ] WebSocket connection manager
- [ ] Price tile component (streaming bid/ask with tick animation)
- [ ] Trade execution UI (click-to-trade, amount selection)
- [ ] Trade blotter (real-time fill table)
- [ ] Session indicator
- [ ] Responsive layout

### Phase 9: Frontend — Internal Dashboard (Weeks 12–14)

**Goal**: Desk monitoring and control

- [ ] Real-time PnL chart with drawdown overlay
- [ ] Position grid with heatmap
- [ ] Client alpha heatmap table
- [ ] Venue ranking board
- [ ] Alert feed with audible notifications
- [ ] Tier management panel (view/edit overrides)
- [ ] Fair value vs. venue quote monitor
- [ ] Session management controls

### Phase 10: Session Management & Polish (Weeks 14–16)

**Goal**: 24h operation model, production-ready feel

- [ ] Session boundary detection and transition logic
- [ ] Per-session configuration loading
- [ ] Session handoff notifications
- [ ] End-of-session PnL snapshots
- [ ] Data retention and archival pipeline
- [ ] Docker Compose for full stack
- [ ] Documentation
- [ ] Load testing and performance benchmarks

---

## 23. Open Decisions

These decisions can be revisited as the project evolves:

### KDB+ Migration Path

QuestDB is the starting choice for ease of setup. To migrate to KDB+:

- Replace QuestDB write path with q IPC calls
- Replace SQL analytics queries with q equivalents
- Use the free KDB+ Personal Edition (32-bit, limited to 4 cores)
- Alternatively, use **PyKX** (Python interface to KDB+) for gradual migration

### Rust as Future Alternative

The plan uses C++ for the engine (industry standard for eFX). If Rust is ever considered:

- The architecture is identical; swap Boost.Asio for tokio, CMake for Cargo
- Rust provides stronger compile-time memory safety guarantees
- Trade-off: fewer FX-specific libraries, smaller talent pool in finance
- Could be considered for greenfield components that don't need C++ library interop

### ZeroMQ vs. Shared Memory

ZeroMQ is simpler to set up and debug. If latency becomes critical:

- Shared memory ring buffers (e.g., using `mmap`) can reduce IPC to < 1μs
- Trade-off: more complex error handling, no built-in pub/sub semantics

### Model Complexity

The alpha model starts with XGBoost. Future options:

- Online learning (update model with each trade, not just daily)
- Neural network for non-linear patterns
- Reinforcement learning for optimal spread setting

---

## Currency Pairs (Starting Set)

### G10 Majors

| Pair   | Pip Size | Primary Venue | Base Spread (pips) |
| ------ | -------- | ------------- | ------------------ |
| EURUSD | 0.00001  | EBS           | 0.2                |
| USDJPY | 0.001    | EBS           | 0.3                |
| GBPUSD | 0.00001  | Reuters       | 0.3                |
| AUDUSD | 0.00001  | Reuters       | 0.4                |
| USDCHF | 0.00001  | EBS           | 0.4                |
| USDCAD | 0.00001  | Reuters       | 0.4                |
| NZDUSD | 0.00001  | Reuters       | 0.5                |

### G10 Crosses

| Pair   | Pip Size | Primary Venue | Base Spread (pips) |
| ------ | -------- | ------------- | ------------------ |
| EURGBP | 0.00001  | Reuters       | 0.4                |
| EURJPY | 0.001    | EBS           | 0.5                |
| GBPJPY | 0.001    | Reuters       | 0.6                |

---

_This document is the living blueprint for the Apex FX platform. Update it as decisions are made and phases are completed._
