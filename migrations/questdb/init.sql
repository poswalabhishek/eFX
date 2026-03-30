-- EFX QuestDB Schema
-- Time-series data: ticks, fills, hedges, PnL

CREATE TABLE IF NOT EXISTS market_ticks (
    timestamp TIMESTAMP,
    venue     SYMBOL,
    pair      SYMBOL,
    bid       DOUBLE,
    ask       DOUBLE,
    bid_size  DOUBLE,
    ask_size  DOUBLE
) TIMESTAMP(timestamp) PARTITION BY HOUR
  WAL
  DEDUP KEYS(timestamp, venue, pair);

CREATE TABLE IF NOT EXISTS fair_values (
    timestamp   TIMESTAMP,
    pair        SYMBOL,
    mid         DOUBLE,
    confidence  DOUBLE,
    volatility  DOUBLE,
    num_venues  INT
) TIMESTAMP(timestamp) PARTITION BY HOUR
  WAL;

CREATE TABLE IF NOT EXISTS fills (
    timestamp     TIMESTAMP,
    trade_id      SYMBOL,
    client_id     SYMBOL,
    pair          SYMBOL,
    side          SYMBOL,
    price         DOUBLE,
    amount        DOUBLE,
    venue         SYMBOL,
    mid_at_fill   DOUBLE,
    spread_bps    DOUBLE,
    session       SYMBOL
) TIMESTAMP(timestamp) PARTITION BY DAY
  WAL;

CREATE TABLE IF NOT EXISTS hedges (
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
) TIMESTAMP(timestamp) PARTITION BY DAY
  WAL;

CREATE TABLE IF NOT EXISTS pnl_snapshots (
    timestamp      TIMESTAMP,
    pair           SYMBOL,
    session        SYMBOL,
    spread_pnl     DOUBLE,
    position_pnl   DOUBLE,
    hedge_cost     DOUBLE,
    total_pnl      DOUBLE,
    position       DOUBLE,
    drawdown       DOUBLE,
    peak_pnl       DOUBLE
) TIMESTAMP(timestamp) PARTITION BY DAY
  WAL;

CREATE TABLE IF NOT EXISTS client_alpha_snapshots (
    timestamp     TIMESTAMP,
    client_id     SYMBOL,
    pair          SYMBOL,
    alpha_1s      DOUBLE,
    alpha_5s      DOUBLE,
    alpha_30s     DOUBLE,
    alpha_60s     DOUBLE,
    alpha_300s    DOUBLE,
    trade_count   INT,
    toxicity      SYMBOL
) TIMESTAMP(timestamp) PARTITION BY DAY
  WAL;

CREATE TABLE IF NOT EXISTS volume_snapshots (
    timestamp     TIMESTAMP,
    venue         SYMBOL,
    pair          SYMBOL,
    session       SYMBOL,
    our_volume    DOUBLE,
    total_volume  DOUBLE,
    market_share  DOUBLE,
    ranking       INT
) TIMESTAMP(timestamp) PARTITION BY DAY
  WAL;
