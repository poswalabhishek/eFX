"use client";

import { Badge } from "@/components/ui/badge";
import type { SessionId } from "@/types/market";

const SESSION_CONFIG: Record<
  SessionId,
  { label: string; city: string; color: string }
> = {
  new_york: { label: "New York", city: "NYC", color: "bg-blue-500" },
  singapore: { label: "Singapore", city: "SGP", color: "bg-amber-500" },
  london: { label: "London", city: "LDN", color: "bg-emerald-500" },
};

function getCurrentSession(): SessionId {
  const utcHour = new Date().getUTCHours();
  if (utcHour >= 6 && utcHour < 14) return "london";
  if (utcHour >= 14 && utcHour < 21) return "new_york";
  return "singapore";
}

function getTimeToNextSession(): string {
  const now = new Date();
  const utcHour = now.getUTCHours();
  const utcMin = now.getUTCMinutes();

  let nextBoundary: number;
  if (utcHour >= 6 && utcHour < 14) nextBoundary = 14;
  else if (utcHour >= 14 && utcHour < 21) nextBoundary = 21;
  else nextBoundary = 6;

  let hoursLeft = nextBoundary - utcHour;
  if (hoursLeft <= 0) hoursLeft += 24;
  const minsLeft = 60 - utcMin;
  if (minsLeft < 60) hoursLeft--;

  return `${hoursLeft}h ${minsLeft}m`;
}

export function SessionIndicator() {
  const session = getCurrentSession();
  const cfg = SESSION_CONFIG[session];
  const timeLeft = getTimeToNextSession();

  return (
    <div className="flex items-center gap-3">
      <div className="flex items-center gap-2">
        <span className={`h-2 w-2 rounded-full ${cfg.color} animate-pulse`} />
        <span className="text-sm font-medium">{cfg.label}</span>
      </div>
      <Badge variant="outline" className="text-xs font-mono">
        {cfg.city}
      </Badge>
      <span className="text-xs text-muted-foreground">
        Next handoff in {timeLeft}
      </span>
    </div>
  );
}
