"""ZMQ subscriber that connects to the C++ pricing engine.

Receives fair value updates and distributes them to WebSocket clients.
Runs as an asyncio background task using zmq.asyncio Poller.
"""

import asyncio
import json
import logging
from collections import defaultdict
from typing import Callable, Awaitable

import zmq
import zmq.asyncio

logger = logging.getLogger("efx.bridge")


class ZmqBridge:
    def __init__(self, engine_address: str = "tcp://localhost:5555"):
        self.engine_address = engine_address
        self._running = False

        self.latest_fair_values: dict[str, dict] = {}
        self.message_count = 0

    async def start(self):
        self._running = True
        ctx = zmq.asyncio.Context()
        socket = ctx.socket(zmq.SUB)
        socket.connect(self.engine_address)
        socket.subscribe(b"")  # subscribe to all topics

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

                    if self.message_count % 500 == 0:
                        logger.info(
                            f"ZMQ: {self.message_count} msgs, "
                            f"{len(self.latest_fair_values)} pairs"
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
