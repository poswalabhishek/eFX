#!/usr/bin/env bash
# EFX Platform Startup Script
# Usage: ./start.sh [--with-docker]

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

echo "=== EFX Platform ==="
echo ""

# Optional: start Docker services
if [[ "$1" == "--with-docker" ]]; then
    echo "[1/4] Starting Docker services (QuestDB, PostgreSQL, Redis)..."
    docker compose up -d
    sleep 3
else
    echo "[1/4] Skipping Docker (use --with-docker to start QuestDB/Postgres/Redis)"
fi

# Build C++ engine if needed
if [[ ! -f engine/build/efx-engine ]]; then
    echo "[2/4] Building C++ engine..."
    cd engine
    cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
    cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc) 2>&1 | tail -3
    cd "$PROJECT_DIR"
else
    echo "[2/4] C++ engine already built"
fi

# Start C++ engine
echo "[3/4] Starting C++ pricing engine..."
./engine/build/efx-engine ./config/ &
ENGINE_PID=$!
echo "  Engine PID: $ENGINE_PID"
sleep 2

# Start Python gateway
echo "[4/4] Starting Python gateway..."
cd gateway
uv run uvicorn gateway.main:app --host 0.0.0.0 --port 8000 &
GATEWAY_PID=$!
echo "  Gateway PID: $GATEWAY_PID"
cd "$PROJECT_DIR"
sleep 3

echo ""
echo "=== EFX Platform Running ==="
echo ""
echo "  Dashboard:  http://localhost:3000/dashboard"
echo "  SDP:        http://localhost:3000/sdp"
echo "  API:        http://localhost:8000/api/health"
echo "  API Docs:   http://localhost:8000/docs"
if [[ "$1" == "--with-docker" ]]; then
    echo "  QuestDB:    http://localhost:9000"
fi
echo ""
echo "  Start frontend:  cd frontend && npm run dev"
echo "  Stop all:        kill $ENGINE_PID $GATEWAY_PID"
echo ""

# Wait for any child to exit
wait
