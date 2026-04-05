# EFX — Electronic FX Market-Making Platform

A full-stack electronic foreign exchange market-making platform built from scratch. Simulates the complete lifecycle of an eFX desk: sourcing market data, constructing fair-value pricing, applying client-specific tiered spreads, executing trades, auto-hedging, and providing real-time analytics.

## Architecture

```
C++ Pricing Engine (2000 ticks/s)
├── Market Data Simulator (4 venues, 10 G10 pairs, jump-diffusion)
├── Fair Value Engine (VWAP + power-smoothed EWMA)
├── Volatility Estimator (real-time realized vol)
├── Client Pricer (8 clients, 5 tiers, per-pair overrides)
├── Order Management System (validation, fills, position limits)
├── Client Simulator (8 archetypes, Poisson arrivals, toxic/benign)
├── Auto-Hedger (best venue selection, slippage simulation)
└── PnL Engine (spread capture + hedge cost + position carry)
    │
    ├── [ZMQ PUB] ──> Python Gateway (FastAPI)
    │                   ├── REST API (clients, tiers, pricing, trades, alpha)
    │                   ├── WebSocket (10Hz dashboard stream)
    │                   └── PostgreSQL + QuestDB
    │
    └── [TCP/ILP] ──> QuestDB (tick storage)

Next.js Frontend
├── /dashboard         — Internal desk dashboard (PnL, positions, prices, alpha, venues, alerts, blotter)
├── /dashboard/clients — Tier management (filter, edit, detail dialogs)
└── /sdp               — Client-facing Single Dealer Platform (click-to-trade)
```

## Quick Start

```bash
# 1. Build the C++ engine
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 2. Start the engine
cd .. && ./engine/build/efx-engine ./config/

# 3. Start the Python gateway (in another terminal)
cd gateway && uv run uvicorn gateway.main:app --host 0.0.0.0 --port 8000

# 4. Start the frontend (in another terminal)
cd frontend && npm run dev

# 5. Open the dashboard
open http://localhost:3000/dashboard
```

Or use the startup script: `./start.sh`

### With Docker (for QuestDB + PostgreSQL + Redis)

```bash
docker compose up -d
./start.sh --with-docker
```

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Pricing Engine | C++20 (Boost, ZeroMQ, spdlog, nlohmann/json, toml++) |
| API Gateway | Python (FastAPI, asyncpg, pyzmq, uvicorn) |
| Analytics | Python (NumPy, Pandas, XGBoost, Plotly, Jupyter) |
| Frontend | Next.js, TypeScript, Tailwind CSS, shadcn/ui, Recharts |
| Tick Database | QuestDB (ILP ingestion, SQL queries) |
| Config Database | PostgreSQL |
| Real-time State | Redis |
| IPC | ZeroMQ (PUB/SUB) |

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/health` | GET | Service health + engine status |
| `/api/clients/` | GET | List all clients |
| `/api/clients/{id}` | GET | Client details |
| `/api/clients/{id}/tier` | PATCH | Change client tier (with audit log) |
| `/api/clients/{id}/overrides` | GET/PUT/DELETE | Pair-level tier overrides |
| `/api/tiers/` | GET | List tier definitions |
| `/api/sessions/` | GET | Trading session configs |
| `/api/pricing/fair-values` | GET | Live fair values |
| `/api/pricing/client-prices/{pair}` | GET | Per-client bid/ask/spread |
| `/api/trade/submit` | POST | Submit trade from SDP |
| `/api/alpha/scores` | GET | Client alpha scores from fills |
| `/api/alpha/current-session` | GET | Current trading session info |
| `/ws/prices` | WS | 10Hz dashboard stream (prices, PnL, positions, venues, alerts, fills) |

## Session Rotation

| Session | UTC Hours | Hub | Spread Multiplier |
|---------|-----------|-----|-------------------|
| London | 06:00–14:00 | London | 1.0x (tightest) |
| New York | 14:00–21:00 | New York | 1.2x |
| Singapore | 21:00–06:00 | Singapore | 1.5x (widest) |

## Client Tiers

| Tier | Multiplier | Typical Client |
|------|-----------|----------------|
| Platinum | 1.0x | High-value, low-alpha (pension funds, sovereigns) |
| Gold | 1.5x | Good clients, moderate volume |
| Silver | 2.5x | Standard clients |
| Bronze | 4.0x | Low volume or mildly toxic |
| Restricted | 8.0x | Highly toxic HFT, under review |

## Project Structure

```
efx/
├── engine/           # C++20 pricing engine
│   ├── core/         # Types, config, ring buffer
│   ├── market_data/  # Simulator, venue book
│   ├── pricing/      # Fair value, spreads, client pricer
│   ├── order/        # OMS, position manager, client simulator
│   ├── hedger/       # Auto-hedger, PnL engine
│   ├── gateway/      # ZMQ publisher, QuestDB writer
│   └── app/          # main() entry point
├── gateway/          # Python FastAPI service
│   └── src/gateway/
│       ├── api/      # REST endpoints
│       ├── ws/       # WebSocket handlers
│       └── bridge/   # ZMQ subscriber, engine state
├── analytics/        # Python analytics + ML
│   └── src/analytics/
│       ├── pnl.py, drawdown.py, volume.py
│       ├── alpha_model.py
│       └── notebooks/
├── frontend/         # Next.js + TypeScript
│   └── src/
│       ├── app/dashboard/    # Internal dashboard
│       ├── app/sdp/          # Client-facing SDP
│       ├── components/       # UI components
│       └── hooks/            # WebSocket + API hooks
├── config/           # TOML configuration files
├── migrations/       # Database schemas
├── docker-compose.yml
└── start.sh
```
