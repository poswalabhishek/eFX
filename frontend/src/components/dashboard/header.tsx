"use client";

import { useEffect, useState } from "react";
import { SessionIndicator } from "./session-indicator";

interface HeaderProps {
  engineConnected: boolean;
  wsConnected: boolean;
}

export function DashboardHeader({ engineConnected, wsConnected }: HeaderProps) {
  const [utcTime, setUtcTime] = useState("");

  useEffect(() => {
    const interval = setInterval(() => {
      setUtcTime(new Date().toISOString().slice(11, 19));
    }, 1000);
    setUtcTime(new Date().toISOString().slice(11, 19));
    return () => clearInterval(interval);
  }, []);

  return (
    <header className="border-b bg-card px-6 py-3">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-4">
          <h1 className="text-lg font-bold tracking-tight">EFX</h1>
          <span className="text-xs text-muted-foreground">Desk Dashboard</span>
        </div>

        <div className="flex items-center gap-6">
          <SessionIndicator />
          <div className="flex items-center gap-2 text-xs text-muted-foreground font-mono">
            <span>UTC</span>
            <span className="font-medium text-foreground">{utcTime}</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span
              className={`h-2 w-2 rounded-full ${
                engineConnected
                  ? "bg-emerald-500"
                  : wsConnected
                    ? "bg-amber-500"
                    : "bg-red-500"
              }`}
            />
            <span className="text-xs text-muted-foreground">
              {engineConnected
                ? "Engine Live"
                : wsConnected
                  ? "Gateway Only"
                  : "Disconnected"}
            </span>
          </div>
        </div>
      </div>
    </header>
  );
}
