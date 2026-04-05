"""EFX Ad-Hoc Analysis Notebook Template

Run with: jupyter notebook (from analytics/ directory)
Or use as a script: python notebooks/efx_analysis.py

Connects to:
  - QuestDB (port 8812) for tick/fill/PnL data
  - PostgreSQL (port 5432) for client/tier config
  - Gateway REST API (port 8000) for live data
"""

# %% Imports
import pandas as pd
import numpy as np
import requests
import plotly.express as px
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from datetime import datetime, timedelta

API_BASE = "http://localhost:8000/api"
QUESTDB_URL = "http://localhost:9000/exec"

# %% Helper functions

def query_questdb(sql: str) -> pd.DataFrame:
    """Query QuestDB via HTTP and return a DataFrame."""
    try:
        r = requests.get(QUESTDB_URL, params={"query": sql, "fmt": "json"})
        data = r.json()
        if "dataset" in data:
            return pd.DataFrame(data["dataset"], columns=[c["name"] for c in data["columns"]])
    except Exception as e:
        print(f"QuestDB not available: {e}")
    return pd.DataFrame()


def get_api(endpoint: str) -> dict | list:
    """Get data from the gateway REST API."""
    r = requests.get(f"{API_BASE}/{endpoint}")
    return r.json()

# %% 1. Live Fair Values
print("=== Current Fair Values ===")
try:
    fv = get_api("pricing/fair-values")
    fv_df = pd.DataFrame([
        {"pair": k, "mid": v["mid"], "confidence": v["confidence"],
         "volatility": v.get("volatility", 0), "venues": v.get("num_venues", 0)}
        for k, v in fv.items()
    ]).sort_values("pair")
    print(fv_df.to_string(index=False))
except Exception as e:
    print(f"Gateway not available: {e}")

# %% 2. Client Alpha Scores
print("\n=== Client Alpha Scores ===")
try:
    alphas = get_api("alpha/scores")
    if alphas:
        alpha_df = pd.DataFrame(alphas).sort_values("score", ascending=False)
        print(alpha_df.to_string(index=False))
    else:
        print("No alpha data (engine may not be running)")
except Exception as e:
    print(f"Error: {e}")

# %% 3. Client Tier Distribution
print("\n=== Client Tier Distribution ===")
try:
    clients = get_api("clients/")
    clients_df = pd.DataFrame(clients)
    tier_dist = clients_df.groupby("default_tier").agg(
        count=("client_id", "count"),
        avg_alpha=("alpha_score", "mean"),
    ).reset_index()
    print(tier_dist.to_string(index=False))
except Exception as e:
    print(f"Error: {e}")

# %% 4. Per-Client EURUSD Pricing
print("\n=== EURUSD Client Pricing ===")
try:
    prices = get_api("pricing/client-prices/EURUSD")
    if prices:
        price_df = pd.DataFrame(prices).sort_values("spread_bps")
        print(price_df[["client_id", "bid", "ask", "spread_bps", "tier"]].to_string(index=False))
except Exception as e:
    print(f"Error: {e}")

# %% 5. Venue Performance
print("\n=== Venue Performance ===")
try:
    health = get_api("health")
    print(f"Engine connected: {health.get('engine_connected')}")
    print(f"ZMQ messages: {health.get('zmq_messages_received')}")
    print(f"Active pairs: {health.get('active_pairs')}")
except Exception as e:
    print(f"Error: {e}")

# %% 6. Current Session
print("\n=== Current Trading Session ===")
try:
    session = get_api("alpha/current-session")
    print(f"Session: {session['label']} ({session['hub_city']})")
    print(f"Next handoff at {session['next_handoff_utc']} UTC")
    print(f"Hours remaining: {session['hours_remaining']}")
except Exception as e:
    print(f"Error: {e}")

# %% 7. QuestDB Queries (when Docker is running)
print("\n=== QuestDB Queries (requires Docker) ===")

# Recent ticks
ticks = query_questdb("SELECT * FROM market_ticks ORDER BY timestamp DESC LIMIT 10")
if not ticks.empty:
    print("Recent ticks:")
    print(ticks.to_string(index=False))
else:
    print("QuestDB not available (start Docker)")

# PnL history
pnl = query_questdb("""
    SELECT timestamp, pair, total_pnl, spread_pnl, drawdown
    FROM pnl_snapshots
    ORDER BY timestamp DESC
    LIMIT 100
""")
if not pnl.empty:
    print("\nPnL history:")
    print(pnl.head(10).to_string(index=False))

print("\n=== Analysis Complete ===")
