"""ZMQ subscriber that connects to the C++ pricing engine.

Receives fair value/tick updates, feeds EngineState for aggregation,
and distributes to WebSocket clients.
"""

import asyncio
import json
import logging

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

                    if topic.startswith("fair_value."):
                        pair = data.get("pair", "")
                        self.latest_fair_values[pair] = data
                        self.state.on_fair_value(data)
                    elif topic.startswith("tick."):
                        self.state.on_tick(data)
                    elif topic.startswith("client_price."):
                        pair = data.get("pair", "")
                        cid = data.get("client_id", "")
                        if pair and cid:
                            if pair not in self.latest_client_prices:
                                self.latest_client_prices[pair] = {}
                            self.latest_client_prices[pair][cid] = data

                    if self.message_count % 1000 == 0:
                        logger.info(
                            f"ZMQ: {self.message_count} msgs, "
                            f"{len(self.latest_fair_values)} pairs, "
                            f"PnL=${self.state.pnl.total:,.0f}"
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
