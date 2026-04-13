"use client";

import { useState, useEffect, useCallback, useRef } from "react";
import type { FairValue, AlertSeverity } from "@/types/market";

const WS_URL = "ws://localhost:8000/ws/prices";

export interface PnlData {
  total: number;
  spread_pnl: number;
  position_pnl: number;
  hedge_cost: number;
  peak: number;
  drawdown: number;
}

export interface PositionData {
  pair: string;
  position: number;
  unrealized_pnl: number;
}

export interface VenueData {
  venue: string;
  volume: string;
  ranking: number;
  pnl: number;
  market_share: number;
}

export interface AlertData {
  id: string;
  severity: AlertSeverity;
  message: string;
  timestamp: number;
}

export interface FillData {
  trade_id: number;
  client_id: string;
  pair: string;
  side: string;
  price: number;
  amount: number;
  venue: string;
  mid_at_fill: number;
  spread_bps: number;
  timestamp: number;
}

export interface ClientPriceData {
  client_id: string;
  pair: string;
  bid: number;
  ask: number;
  spread_bps: number;
  tier: string;
}

export interface HistoryPoint {
  ts: number;
  total: number;
}

interface DashboardState {
  pairs: Record<string, FairValue>;
  pnl: PnlData;
  positions: PositionData[];
  venues: VenueData[];
  alerts: AlertData[];
  fills: FillData[];
  clientPrices: Record<string, Record<string, ClientPriceData>>;
  pnlHistory: HistoryPoint[];
  connected: boolean;
  engineConnected: boolean;
  manualMode: boolean;
  lastUpdate: number;
}

const EMPTY_PNL: PnlData = {
  total: 0, spread_pnl: 0, position_pnl: 0, hedge_cost: 0, peak: 0, drawdown: 0,
};

export function useDashboard(): DashboardState {
  const [state, setState] = useState<DashboardState>({
    pairs: {},
    pnl: EMPTY_PNL,
    positions: [],
    venues: [],
    alerts: [],
    fills: [],
    clientPrices: {},
    pnlHistory: [],
    connected: false,
    engineConnected: false,
    manualMode: false,
    lastUpdate: 0,
  });

  const wsRef = useRef<WebSocket | null>(null);
  const retriesRef = useRef(0);
  const reconnectRef = useRef<() => void>(() => {});

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return;

    const ws = new WebSocket(WS_URL);

    ws.onopen = () => {
      setState((prev) => ({ ...prev, connected: true }));
      retriesRef.current = 0;
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        if (data.type === "dashboard") {
          const prices: Record<string, FairValue> = {};
          if (data.prices) {
            for (const [pair, fv] of Object.entries(data.prices)) {
              const raw = fv as Record<string, unknown>;
              prices[pair] = {
                pair,
                mid: raw.mid as number,
                confidence: raw.confidence as number,
                volatility: raw.volatility as number,
                num_venues: raw.num_venues as number,
                timestamp: raw.timestamp as number,
              };
            }
          }

          setState((prev) => {
            const nextPnl = data.pnl ?? prev.pnl;
            const nextHistory = nextPnl
              ? [...prev.pnlHistory, { ts: Date.now(), total: nextPnl.total }].slice(-120)
              : prev.pnlHistory;

            return {
              ...prev,
              pairs: prices,
              pnl: nextPnl,
              positions: data.positions ?? prev.positions,
              venues: data.venues ?? prev.venues,
              alerts: data.alerts ?? prev.alerts,
              fills: data.fills ?? prev.fills,
              clientPrices: data.client_prices ?? prev.clientPrices,
              pnlHistory: nextHistory,
              engineConnected: true,
              manualMode: data.manual_mode ?? prev.manualMode,
              lastUpdate: Date.now(),
            };
          });
        } else if (data.type === "heartbeat") {
          setState((prev) => ({
            ...prev,
            engineConnected: data.engine_connected ?? false,
            manualMode: data.manual_mode ?? prev.manualMode,
          }));
        }
      } catch {
        // ignore parse errors
      }
    };

    ws.onclose = () => {
      setState((prev) => ({ ...prev, connected: false }));
      if (retriesRef.current < 20) {
        retriesRef.current++;
        const delay = Math.min(1000 * Math.pow(1.5, retriesRef.current), 10000);
        setTimeout(() => reconnectRef.current(), delay);
      }
    };

    ws.onerror = () => ws.close();
    wsRef.current = ws;
  }, []);

  useEffect(() => {
    reconnectRef.current = connect;
  }, [connect]);

  useEffect(() => {
    connect();
    return () => {
      wsRef.current?.close();
    };
  }, [connect]);

  return state;
}
