"use client";

import { useState, useEffect, useCallback } from "react";
import type { Client, Tier } from "@/types/market";

const API_BASE = "http://localhost:8000/api";

export function useClients() {
  const [clients, setClients] = useState<Client[]>([]);
  const [tiers, setTiers] = useState<Tier[]>([]);
  const [loading, setLoading] = useState(true);

  const fetchClients = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/clients/`);
      if (res.ok) {
        const data = await res.json();
        setClients(data);
      }
    } catch {
      // gateway not available
    }
  }, []);

  const fetchTiers = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/tiers/`);
      if (res.ok) {
        setTiers(await res.json());
      }
    } catch {
      // gateway not available
    }
  }, []);

  const updateClientTier = useCallback(
    async (clientId: string, tier: string, reason?: string) => {
      try {
        const res = await fetch(`${API_BASE}/clients/${clientId}/tier`, {
          method: "PATCH",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ tier, reason, changed_by: "analyst" }),
        });
        if (res.ok) {
          await fetchClients();
          return true;
        }
      } catch {
        // ignore
      }
      return false;
    },
    [fetchClients],
  );

  useEffect(() => {
    Promise.all([fetchClients(), fetchTiers()]).then(() => setLoading(false));
    const interval = setInterval(fetchClients, 10000);
    return () => clearInterval(interval);
  }, [fetchClients, fetchTiers]);

  return { clients, tiers, loading, updateClientTier, refetch: fetchClients };
}
