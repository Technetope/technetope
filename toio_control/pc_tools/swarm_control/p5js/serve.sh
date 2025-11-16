#!/bin/bash
# Toio Swarm Simulation サーバー起動スクリプト

cd "$(dirname "$0")"

echo "=========================================="
echo "Toio Swarm Simulation Server"
echo "=========================================="
echo ""
echo "Starting server on http://localhost:8000"
echo "Open http://localhost:8000/toio_swarm_sim.html in your browser"
echo ""
echo "Press Ctrl+C to stop the server"
echo "=========================================="
echo ""

python3 -m http.server 8000

