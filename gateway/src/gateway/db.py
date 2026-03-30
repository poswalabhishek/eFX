"""Database connection pool and helper functions using asyncpg."""

import logging
from typing import Any

import asyncpg

from gateway.config import settings

logger = logging.getLogger("efx.db")

_pool: asyncpg.Pool | None = None


async def get_pool() -> asyncpg.Pool:
    global _pool
    if _pool is None:
        try:
            _pool = await asyncpg.create_pool(
                host="localhost",
                port=5432,
                user="efx",
                password="efx_dev",
                database="efx",
                min_size=2,
                max_size=10,
                command_timeout=10,
            )
            logger.info("PostgreSQL connection pool created")
        except Exception as e:
            logger.warning(f"PostgreSQL not available: {e}")
            raise
    return _pool


async def close_pool():
    global _pool
    if _pool:
        await _pool.close()
        _pool = None
        logger.info("PostgreSQL pool closed")


async def fetch_all(query: str, *args: Any) -> list[dict]:
    pool = await get_pool()
    rows = await pool.fetch(query, *args)
    return [dict(r) for r in rows]


async def fetch_one(query: str, *args: Any) -> dict | None:
    pool = await get_pool()
    row = await pool.fetchrow(query, *args)
    return dict(row) if row else None


async def execute(query: str, *args: Any) -> str:
    pool = await get_pool()
    return await pool.execute(query, *args)
