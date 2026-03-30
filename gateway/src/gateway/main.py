from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from gateway.config import settings
from gateway.api.clients import router as clients_router
from gateway.api.tiers import router as tiers_router
from gateway.api.health import router as health_router
from gateway.ws.prices import router as ws_prices_router


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    yield
    # Shutdown


app = FastAPI(
    title=settings.app_name,
    version="0.1.0",
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
app.include_router(ws_prices_router, prefix="/ws", tags=["websocket"])
