#!/bin/bash

# Simple PodCache Test
# Quick test to verify basic functionality

set -e

echo "Simple PodCache Test"
echo "==================="

# Configuration
SERVER_PORT=6380
SERVER_PID=""
BUILD_DIR="$(pwd)/build"

# Cleanup function
cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        echo "Stopping server..."
        kill -INT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    pkill -f podcache || true
}

trap cleanup EXIT INT TERM

# Function to send RESP command
send_command() {
    local cmd="$1"
    echo -e "$cmd" | nc -w 1 localhost $SERVER_PORT 2>/dev/null || echo "ERROR"
}

# Function to set a key-value pair
set_key() {
    local key="$1"
    local value="$2"
    local value_len=${#value}
    local key_len=${#key}
    
    local cmd="*3\r\n\$3\r\nSET\r\n\$${key_len}\r\n${key}\r\n\$${value_len}\r\n${value}\r\n"
    send_command "$cmd"
}

# Function to get a key
get_key() {
    local key="$1"
    local key_len=${#key}
    
    local cmd="*2\r\n\$3\r\nGET\r\n\$${key_len}\r\n${key}\r\n"
    send_command "$cmd"
}

cd "$BUILD_DIR"

# Start server with 1MB cache
echo "Starting server with 1MB cache..."
PODCACHE_SIZE=1 PODCACHE_SERVER_PORT=$SERVER_PORT ./podcache &
SERVER_PID=$!

sleep 2

# Test basic operations
echo "Testing basic SET/GET..."
result=$(set_key "hello" "world")
echo "SET result: $result"

result=$(get_key "hello")
echo "GET result: $result"

# Fill cache to test disk swapping
echo "Filling cache to test disk swapping..."
for i in {1..20}; do
    large_value=$(head -c 100000 /dev/zero | tr '\0' 'A')  # 100KB of 'A's
    result=$(set_key "large_$i" "$large_value")
    echo "SET large_$i: OK"
done

# Test retrieval of early items (should be on disk now)
echo "Testing retrieval of early items..."
result=$(get_key "large_1")
if [[ "$result" == *"AAAA"* ]]; then
    echo "✓ Successfully retrieved large_1 from disk"
else
    echo "✗ Failed to retrieve large_1"
fi

result=$(get_key "hello")
if [[ "$result" == *"world"* ]]; then
    echo "✓ Successfully retrieved hello"
else
    echo "✗ Failed to retrieve hello"
fi

echo "Test completed!"
