"use client";

import { useState, useEffect, useCallback, useRef } from "react";
import type { FairValue } from "@/types/market";

const WS_URL = "ws://localhost:8000/ws/prices";

interface PriceState {
  pairs: Record<string, FairValue>;
  connected: boolean;
  engineConnected: boolean;
  lastUpdate: number;
}

export function usePrices(): PriceState {
  const [state, setState] = useState<PriceState>({
    pairs: {},
    connected: false,
    engineConnected: false,
    lastUpdate: 0,
  });

  const wsRef = useRef<WebSocket | null>(null);
  const retriesRef = useRef(0);

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
        if (data.type === "prices" && data.pairs) {
          const pairs: Record<string, FairValue> = {};
          for (const [pair, fv] of Object.entries(data.pairs)) {
            const raw = fv as Record<string, unknown>;
            pairs[pair] = {
              pair,
              mid: raw.mid as number,
              confidence: raw.confidence as number,
              volatility: raw.volatility as number,
              num_venues: raw.num_venues as number,
              timestamp: raw.timestamp as number,
            };
          }
          setState((prev) => ({
            ...prev,
            pairs,
            engineConnected: true,
            lastUpdate: Date.now(),
          }));
        } else if (data.type === "heartbeat") {
          setState((prev) => ({
            ...prev,
            engineConnected: data.engine_connected ?? false,
          }));
        }
      } catch {
        // ignore malformed messages
      }
    };

    ws.onclose = () => {
      setState((prev) => ({ ...prev, connected: false }));
      if (retriesRef.current < 20) {
        retriesRef.current++;
        const delay = Math.min(1000 * Math.pow(1.5, retriesRef.current), 10000);
        setTimeout(connect, delay);
      }
    };

    ws.onerror = () => ws.close();
    wsRef.current = ws;
  }, []);

  useEffect(() => {
    connect();
    return () => {
      wsRef.current?.close();
    };
  }, [connect]);

  return state;
}
