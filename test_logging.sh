#!/bin/bash

echo "=== Testing PodCache Logging System ==="

cd build

# Remove old log
rm -f podcache.log

# Start server
echo "Starting PodCache server..."
./podcache &
SERVER_PID=$!

sleep 2

# Test RESP commands with netcat
echo "Testing RESP operations..."

# SET command
echo -e '*3\r\n$3\r\nSET\r\n$4\r\ntest\r\n$5\r\nvalue\r\n' | nc localhost 6379 &
sleep 0.5

# GET command
echo -e '*2\r\n$3\r\nGET\r\n$4\r\ntest\r\n' | nc localhost 6379 &
sleep 0.5

# INCR command
echo -e '*3\r\n$3\r\nSET\r\n$7\r\ncounter\r\n$1\r\n5\r\n' | nc localhost 6379 &
sleep 0.5
echo -e '*2\r\n$4\r\nINCR\r\n$7\r\ncounter\r\n' | nc localhost 6379 &
sleep 0.5

# DEL command
echo -e '*2\r\n$3\r\nDEL\r\n$4\r\ntest\r\n' | nc localhost 6379 &
sleep 0.5

echo "Operations completed, waiting for server..."
sleep 2

# Stop server
echo "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "=== COMPLETE LOG OUTPUT ==="
cat podcache.log

echo ""
echo "=== Test completed ==="
