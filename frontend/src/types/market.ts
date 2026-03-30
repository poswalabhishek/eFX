export type Side = "BUY" | "SELL";

export type TierId =
  | "platinum"
  | "gold"
  | "silver"
  | "bronze"
  | "restricted";

export type Toxicity = "low" | "medium" | "high" | "extreme";

export type SessionId = "new_york" | "singapore" | "london";

export interface PriceTick {
  pair: string;
  bid: number;
  ask: number;
  mid: number;
  spread_bps: number;
  timestamp: number;
}

export interface FairValue {
  pair: string;
  mid: number;
  confidence: number;
  volatility: number;
  num_venues: number;
  timestamp: number;
}

export interface Position {
  pair: string;
  amount: number;
  avg_price: number;
  unrealized_pnl: number;
  timestamp: number;
}

export interface PnlSnapshot {
  timestamp: number;
  pair: string;
  session: SessionId;
  spread_pnl: number;
  position_pnl: number;
  hedge_cost: number;
  total_pnl: number;
  position: number;
  drawdown: number;
  peak_pnl: number;
}

export interface Client {
  client_id: string;
  name: string;
  client_type: string;
  default_tier: TierId;
  alpha_score: number;
  toxicity: Toxicity;
  is_important: boolean;
  is_active: boolean;
  notes?: string;
}

export interface ClientPairOverride {
  client_id: string;
  pair: string;
  tier_override: TierId;
  adjustment_pct: number;
}

export interface VenuePerformance {
  venue: string;
  volume: number;
  total_volume: number;
  market_share: number;
  ranking: number;
  pnl: number;
}

export interface Fill {
  trade_id: string;
  client_id: string;
  pair: string;
  side: Side;
  price: number;
  amount: number;
  venue: string;
  mid_at_fill: number;
  spread_bps: number;
  session: SessionId;
  timestamp: number;
}

export type AlertSeverity = "info" | "warning" | "critical" | "emergency";

export interface Alert {
  id: string;
  severity: AlertSeverity;
  message: string;
  timestamp: number;
  acknowledged: boolean;
}

export interface Tier {
  tier_id: TierId;
  label: string;
  multiplier: number;
}

export interface SessionInfo {
  id: SessionId;
  label: string;
  hub_city: string;
  start_utc: string;
  end_utc: string;
  is_active: boolean;
}
