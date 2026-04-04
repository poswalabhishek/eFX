import asyncio
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from gateway.config import settings
from gateway.api.clients import router as clients_router
from gateway.api.tiers import router as tiers_router
from gateway.api.sessions import router as sessions_router
from gateway.api.pricing import router as pricing_router, set_pricing_bridge
from gateway.api.trade import router as trade_router, set_trade_bridge
from gateway.api.health import router as health_router, set_health_bridge
from gateway.ws.prices import router as ws_prices_router, set_bridge
from gateway.bridge.zmq_subscriber import ZmqBridge
from gateway import db

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(levelname)s: %(message)s")
logger = logging.getLogger("efx.gateway")

bridge = ZmqBridge(settings.zmq_price_sub)


@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("Starting EFX Gateway v0.3.0...")

    # Try connecting to Postgres (non-fatal if unavailable)
    try:
        await db.get_pool()
    except Exception as e:
        logger.warning(f"Postgres not available at startup: {e}")

    set_bridge(bridge)
    set_health_bridge(bridge)
    set_pricing_bridge(bridge)
    set_trade_bridge(bridge)
    bridge_task = asyncio.create_task(bridge.start())

    yield

    bridge.stop()
    bridge_task.cancel()
    try:
        await bridge_task
    except asyncio.CancelledError:
        pass
    await db.close_pool()
    logger.info("EFX Gateway stopped.")


app = FastAPI(
    title=settings.app_name,
    version="0.3.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:3000"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(health_router, prefix="/api", tags=["health"])
app.include_router(clients_router, prefix="/api/clients", tags=["clients"])
app.include_router(tiers_router, prefix="/api/tiers", tags=["tiers"])
app.include_router(sessions_router, prefix="/api/sessions", tags=["sessions"])
app.include_router(pricing_router, prefix="/api/pricing", tags=["pricing"])
app.include_router(trade_router, prefix="/api/trade", tags=["trade"])
app.include_router(ws_prices_router, prefix="/ws", tags=["websocket"])
