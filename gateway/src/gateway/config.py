from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    app_name: str = "EFX Gateway"
    debug: bool = True

    # Database
    postgres_url: str = "postgresql+asyncpg://efx:efx_dev@localhost:5432/efx"
    postgres_sync_url: str = "postgresql://efx:efx_dev@localhost:5432/efx"
    questdb_host: str = "localhost"
    questdb_http_port: int = 9000
    questdb_pg_port: int = 8812
    questdb_ilp_port: int = 9009

    # Redis
    redis_url: str = "redis://localhost:6379"

    # ZMQ (connects to C++ engine)
    zmq_price_sub: str = "tcp://localhost:5555"
    zmq_trade_pub: str = "tcp://localhost:5556"

    # Server
    host: str = "0.0.0.0"
    port: int = 8000
    ws_heartbeat_interval: int = 5

    model_config = {"env_prefix": "EFX_"}


settings = Settings()
