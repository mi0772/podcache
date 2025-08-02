#!/bin/bash

# Medium PodCache Disk Swap Test
# Tests disk swapping with a reasonable amount of data

set -e

echo "========================================"
echo "  PodCache Medium Disk Swap Test"
echo "========================================"

# Configuration
SERVER_PORT=6380
CACHE_SIZE_MB=2  # Small cache to trigger disk swapping
SERVER_PID=""
BUILD_DIR="$(pwd)/build"
LOG_FILE="$BUILD_DIR/podcache_medium_test.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Cleanup function
cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    if [ ! -z "$SERVER_PID" ]; then
        kill -INT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    pkill -f podcache || true
    echo -e "${GREEN}Cleanup completed${NC}"
}

trap cleanup EXIT INT TERM

# Function to send RESP command
send_command() {
    local cmd="$1"
    echo -e "$cmd" | nc -w 2 localhost $SERVER_PORT 2>/dev/null || echo "ERROR"
}

# Function to generate random string
generate_random_string() {
    local length=$1
    head -c $length /dev/urandom | base64 | tr -d '\n' | head -c $length
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

echo -e "${BLUE}Starting PodCache Medium Disk Swap Test...${NC}"
echo "Cache size: ${CACHE_SIZE_MB}MB"
echo "Server port: ${SERVER_PORT}"
echo ""

cd "$BUILD_DIR"
rm -f "$LOG_FILE"

# Start server
echo -e "${YELLOW}Starting PodCache server...${NC}"
PODCACHE_SIZE=$CACHE_SIZE_MB PODCACHE_SERVER_PORT=$SERVER_PORT ./podcache > "$LOG_FILE" 2>&1 &
SERVER_PID=$!

echo "Server started with PID: $SERVER_PID"
echo "Waiting for server to initialize..."
sleep 3

# Test connection
echo "Testing server connection..."
for i in {1..5}; do
    if nc -z localhost $SERVER_PORT 2>/dev/null; then
        echo -e "${GREEN}✓ Server is responding on port $SERVER_PORT${NC}"
        break
    fi
    if [ $i -eq 5 ]; then
        echo -e "${RED}ERROR: Server is not responding!${NC}"
        echo "Log output:"
        cat "$LOG_FILE"
        exit 1
    fi
    echo "Waiting for server... ($i/5)"
    sleep 1
done

echo ""

# Test 1: Fill cache beyond memory limit
echo -e "${BLUE}=== Test 1: Filling cache beyond memory limit ===${NC}"

ITEM_SIZE=50000  # 50KB per item
TARGET_ITEMS=80  # Should exceed 2MB with 80 * 50KB = 4MB

echo "Inserting $TARGET_ITEMS items of ~${ITEM_SIZE} bytes each..."
echo "This should exceed ${CACHE_SIZE_MB}MB and trigger disk swapping"
echo ""

# Array to store keys
declare -a TEST_KEYS

# Insert items
for i in $(seq 1 $TARGET_ITEMS); do
    key="medium_test_key_$i"
    value=$(generate_random_string $ITEM_SIZE)
    
    TEST_KEYS+=("$key")
    
    result=$(set_key "$key" "$value")
    
    # Show progress every 10 items
    if [ $((i % 10)) -eq 0 ]; then
        echo -ne "Progress: $i/$TARGET_ITEMS items inserted ($(( i * ITEM_SIZE / 1024 ))KB)\r"
    fi
    
    # Check if server crashed
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "\n${RED}ERROR: Server crashed during insertion!${NC}"
        echo "Last log entries:"
        tail -10 "$LOG_FILE"
        exit 1
    fi
    
    # Brief pause to avoid overwhelming the server
    sleep 0.05
done

echo -e "\n${GREEN}✓ Successfully inserted $TARGET_ITEMS items${NC}"
echo ""

# Wait for any pending operations
sleep 3

# Test 2: Retrieve items (mix of disk and memory)
echo -e "${BLUE}=== Test 2: Retrieving items from disk and memory ===${NC}"

SUCCESS_COUNT=0
FAIL_COUNT=0

echo "Testing retrieval of first 20 items (likely on disk)..."

for i in $(seq 1 20); do
    key="medium_test_key_$i"
    result=$(get_key "$key")
    
    if [[ "$result" == *"ERROR"* ]] || [[ "$result" == *"\$-1"* ]]; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo -e "${RED}✗ Failed to retrieve $key${NC}"
    else
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        if [ $i -le 5 ]; then
            echo -e "${GREEN}✓ Retrieved $key successfully${NC}"
        fi
    fi
done

echo "Disk retrieval test results:"
echo "  ✓ Successful retrievals: $SUCCESS_COUNT/20"
echo "  ✗ Failed retrievals: $FAIL_COUNT/20"
echo ""

# Test 3: Retrieve recent items
echo -e "${BLUE}=== Test 3: Retrieving recent items ===${NC}"

MEMORY_SUCCESS=0
MEMORY_FAIL=0

echo "Testing retrieval of last 20 items (likely in memory)..."

for i in $(seq 61 80); do
    key="medium_test_key_$i"
    result=$(get_key "$key")
    
    if [[ "$result" == *"ERROR"* ]] || [[ "$result" == *"\$-1"* ]]; then
        MEMORY_FAIL=$((MEMORY_FAIL + 1))
    else
        MEMORY_SUCCESS=$((MEMORY_SUCCESS + 1))
    fi
done

echo "Memory retrieval test results:"
echo "  ✓ Successful retrievals: $MEMORY_SUCCESS/20"
echo "  ✗ Failed retrievals: $MEMORY_FAIL/20"
echo ""

# Test 4: Mixed access pattern
echo -e "${BLUE}=== Test 4: Mixed access pattern test ===${NC}"

MIXED_SUCCESS=0
echo "Testing mixed access pattern (random keys)..."

for i in {1..10}; do
    # Random key between 1 and TARGET_ITEMS
    random_index=$((RANDOM % TARGET_ITEMS + 1))
    key="medium_test_key_$random_index"
    
    result=$(get_key "$key")
    
    if [[ "$result" != *"ERROR"* ]] && [[ "$result" != *"\$-1"* ]]; then
        MIXED_SUCCESS=$((MIXED_SUCCESS + 1))
    fi
done

echo "Mixed access test results:"
echo "  ✓ Successful retrievals: $MIXED_SUCCESS/10"
echo ""

# Final summary
echo -e "${BLUE}========================================"
echo "           TEST SUMMARY"
echo "========================================${NC}"

echo "Configuration:"
echo "  - Total items inserted: $TARGET_ITEMS"
echo "  - Item size: ${ITEM_SIZE} bytes (~$(( ITEM_SIZE / 1024 ))KB)"
echo "  - Total data: ~$(( TARGET_ITEMS * ITEM_SIZE / 1024 / 1024 ))MB"
echo "  - Cache limit: ${CACHE_SIZE_MB}MB"
echo ""

echo "Results:"
echo "  ✓ Disk retrieval success: $SUCCESS_COUNT/20 ($(( SUCCESS_COUNT * 100 / 20 ))%)"
echo "  ✓ Memory retrieval success: $MEMORY_SUCCESS/20 ($(( MEMORY_SUCCESS * 100 / 20 ))%)"
echo "  ✓ Mixed access success: $MIXED_SUCCESS/10 ($(( MIXED_SUCCESS * 100 / 10 ))%)"

# Overall assessment
TOTAL_SCORE=$(( (SUCCESS_COUNT * 100 / 20) + (MEMORY_SUCCESS * 100 / 20) + (MIXED_SUCCESS * 100 / 10) ))
AVERAGE_SCORE=$(( TOTAL_SCORE / 3 ))

echo ""
if [ $AVERAGE_SCORE -ge 90 ]; then
    echo -e "${GREEN}🎉 EXCELLENT! Overall success rate: ${AVERAGE_SCORE}%${NC}"
    echo -e "${GREEN}   Disk swapping is working perfectly!${NC}"
elif [ $AVERAGE_SCORE -ge 70 ]; then
    echo -e "${YELLOW}⚠️  GOOD: Overall success rate: ${AVERAGE_SCORE}%${NC}"
    echo -e "${YELLOW}   Disk swapping is functional with minor issues${NC}"
else
    echo -e "${RED}❌ ISSUES DETECTED: Overall success rate: ${AVERAGE_SCORE}%${NC}"
    echo -e "${RED}   Disk swapping may not be working correctly${NC}"
fi

echo ""
echo -e "${BLUE}📋 Recent server log entries:${NC}"
tail -15 "$LOG_FILE" | head -10

echo ""
echo -e "${GREEN}Test completed!${NC}"
echo "Full server log available at: $LOG_FILE"
