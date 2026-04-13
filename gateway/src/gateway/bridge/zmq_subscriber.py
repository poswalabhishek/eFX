"""ZMQ subscriber that connects to the C++ pricing engine.

Receives fair value/tick updates, feeds EngineState for aggregation,
and distributes to WebSocket clients.
"""

import asyncio
import json
import logging
import time

import zmq
import zmq.asyncio

from gateway.bridge.engine_state import EngineState

logger = logging.getLogger("efx.bridge")


class ZmqBridge:
    def __init__(self, engine_address: str = "tcp://localhost:5555"):
        self.engine_address = engine_address
        self._running = False

        self.latest_fair_values: dict[str, dict] = {}
        self.latest_client_prices: dict[str, dict[str, dict]] = {}  # pair -> client_id -> price
        self.message_count = 0
        self.state = EngineState()
        self.manual_mode = False

    def set_manual_mode(self, enabled: bool):
        self.manual_mode = enabled
        self.state.set_manual_mode(enabled)
        logger.info("Manual market mode %s", "enabled" if enabled else "disabled")

    def apply_manual_trade_impact(self, pair: str, side: str, amount: float):
        fv = self.latest_fair_values.get(pair)
        if not fv:
            return

        amount_m = max(0.1, abs(amount) / 1_000_000.0)
        impact_bps = min(12.0, 0.4 * (amount_m ** 0.5))
        direction = 1.0 if side == "BUY" else -1.0
        px_mult = 1.0 + direction * impact_bps / 10000.0

        old_mid = float(fv.get("mid", 0.0))
        if old_mid <= 0:
            return

        new_mid = old_mid * px_mult
        new_ts = int(time.time() * 1_000_000)

        fv["mid"] = new_mid
        fv["timestamp"] = new_ts
        self.latest_fair_values[pair] = fv
        self.state.on_fair_value(fv)

        pair_prices = self.latest_client_prices.get(pair, {})
        for cid, cp in pair_prices.items():
            bid = float(cp.get("bid", new_mid))
            ask = float(cp.get("ask", new_mid))
            spread = max(ask - bid, new_mid * 0.000001)
            shifted_mid = ((bid + ask) / 2.0) * px_mult
            cp["bid"] = shifted_mid - spread / 2.0
            cp["ask"] = shifted_mid + spread / 2.0
            cp["timestamp"] = new_ts
            pair_prices[cid] = cp
        self.latest_client_prices[pair] = pair_prices

    async def start(self):
        self._running = True
        ctx = zmq.asyncio.Context()
        socket = ctx.socket(zmq.SUB)
        socket.connect(self.engine_address)
        socket.subscribe(b"")

        poller = zmq.asyncio.Poller()
        poller.register(socket, zmq.POLLIN)

        logger.info(f"ZMQ bridge connected to {self.engine_address}")

        try:
            while self._running:
                events = dict(await poller.poll(timeout=500))
                if socket in events:
                    parts = await socket.recv_multipart(zmq.NOBLOCK)
                    if len(parts) != 2:
                        continue

                    topic = parts[0].decode()
                    data = json.loads(parts[1].decode())
                    self.message_count += 1

                    if self.manual_mode and (
                        topic.startswith("fair_value.")
                        or topic.startswith("tick.")
                        or topic.startswith("client_price.")
                        or topic.startswith("fill.")
                        or topic == "positions"
                        or topic == "pnl"
                    ):
                        continue

                    if topic.startswith("fair_value."):
                        pair = data.get("pair", "")
                        self.latest_fair_values[pair] = data
                        self.state.on_fair_value(data)
                    elif topic.startswith("tick."):
                        self.state.on_tick(data)
                    elif topic.startswith("fill."):
                        self.state.on_fill(data)
                    elif topic.startswith("hedge."):
                        self.state.on_hedge(data)
                    elif topic == "positions":
                        self.state.on_positions(data)
                    elif topic == "pnl":
                        self.state.on_engine_pnl(data)
                    elif topic.startswith("client_price."):
                        pair = data.get("pair", "")
                        cid = data.get("client_id", "")
                        if pair and cid:
                            if pair not in self.latest_client_prices:
                                self.latest_client_prices[pair] = {}
                            self.latest_client_prices[pair][cid] = data

                    if self.message_count % 1000 == 0:
                        total_pnl = self.state.realized_pnl + self.state.unrealized_pnl
                        logger.info(
                            f"ZMQ: {self.message_count} msgs, "
                            f"{len(self.latest_fair_values)} pairs, "
                            f"fills={self.state.total_fills}, "
                            f"PnL=${total_pnl:,.0f}"
                        )
        except asyncio.CancelledError:
            pass
        except Exception as e:
            logger.error(f"ZMQ bridge error: {e}", exc_info=True)
        finally:
            poller.unregister(socket)
            socket.close()
            ctx.term()
            logger.info(f"ZMQ bridge stopped. Total msgs: {self.message_count}")

    def stop(self):
        self._running = False
