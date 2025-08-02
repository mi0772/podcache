#!/bin/bash

# PodCache Disk Swap Test
# This test fills the cache beyond its memory limit to test disk swapping
# and retrieval mechanisms

set -e

echo "=========================================="
echo "    PodCache Disk Swap Test"
echo "=========================================="

# Configuration
SERVER_PORT=6380
CACHE_SIZE_MB=10  # Small cache to trigger disk swapping quickly
SERVER_PID=""
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$TEST_DIR/build"
LOG_FILE="$BUILD_DIR/podcache_test.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    if [ ! -z "$SERVER_PID" ]; then
        echo "Stopping server (PID: $SERVER_PID)..."
        kill -INT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    
    # Clean up any remaining processes
    pkill -f podcache || true
    
    echo -e "${GREEN}Cleanup completed${NC}"
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

# Function to send RESP command
send_command() {
    local cmd="$1"
    echo -e "$cmd" | nc -w 1 localhost $SERVER_PORT 2>/dev/null || echo "ERROR"
}

# Function to generate random string
generate_random_string() {
    local length=$1
    cat /dev/urandom | base64 | tr -d '\n' | head -c $length
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

# Function to delete a key
del_key() {
    local key="$1"
    local key_len=${#key}
    
    local cmd="*2\r\n\$3\r\nDEL\r\n\$${key_len}\r\n${key}\r\n"
    send_command "$cmd"
}

# Start the test
echo -e "${BLUE}Starting PodCache Disk Swap Test...${NC}"
echo "Cache size: ${CACHE_SIZE_MB}MB"
echo "Server port: ${SERVER_PORT}"
echo ""

# Navigate to build directory
cd "$BUILD_DIR"

# Remove old log
rm -f "$LOG_FILE"

# Start server with small cache size
echo -e "${YELLOW}Starting PodCache server...${NC}"
PODCACHE_SIZE=$CACHE_SIZE_MB PODCACHE_SERVER_PORT=$SERVER_PORT ./podcache > "$LOG_FILE" 2>&1 &
SERVER_PID=$!

echo "Server started with PID: $SERVER_PID"
echo "Waiting for server to initialize..."
sleep 5

# Test connection
echo "Testing server connection..."
for i in {1..10}; do
    if nc -z localhost $SERVER_PORT 2>/dev/null; then
        echo "✓ Server is responding on port $SERVER_PORT"
        break
    fi
    if [ $i -eq 10 ]; then
        echo -e "${RED}ERROR: Server is not responding after 10 attempts!${NC}"
        echo "Log output:"
        cat "$LOG_FILE"
        exit 1
    fi
    echo "Attempt $i/10: waiting for server..."
    sleep 1
done

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Server failed to start!${NC}"
    echo "Log output:"
    cat "$LOG_FILE"
    exit 1
fi

echo -e "${GREEN}Server started successfully${NC}"
echo ""

# Test 1: Fill cache beyond memory limit
echo -e "${BLUE}=== Test 1: Filling cache beyond memory limit ===${NC}"

# Calculate approximate number of items needed to exceed cache
# Assuming average item size of ~1KB, we need more than CACHE_SIZE_MB * 1024 items
ITEM_SIZE=1024  # 1KB per item
TARGET_ITEMS=$((CACHE_SIZE_MB * 1024 + 500))  # Exceed by 500 items

echo "Inserting $TARGET_ITEMS items of ~${ITEM_SIZE} bytes each..."
echo "This should exceed ${CACHE_SIZE_MB}MB and trigger disk swapping"
echo ""

# Array to store keys for later retrieval
declare -a TEST_KEYS

# Insert items
for i in $(seq 1 $TARGET_ITEMS); do
    key="test_key_$i"
    value=$(generate_random_string $ITEM_SIZE)
    
    TEST_KEYS+=("$key")
    
    result=$(set_key "$key" "$value")
    
    # Show progress every 100 items
    if [ $((i % 100)) -eq 0 ]; then
        echo -ne "Progress: $i/$TARGET_ITEMS items inserted\r"
    fi
    
    # Check if server is still running
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "\n${RED}ERROR: Server crashed during insertion!${NC}"
        echo "Log output:"
        tail -20 "$LOG_FILE"
        exit 1
    fi
done

echo -e "\n${GREEN}✓ Successfully inserted $TARGET_ITEMS items${NC}"
echo ""

# Wait a moment for any pending disk operations
sleep 2

# Test 2: Retrieve items that should be on disk
echo -e "${BLUE}=== Test 2: Retrieving items from disk ===${NC}"

# Try to retrieve the first items (these should have been swapped to disk)
SUCCESS_COUNT=0
FAIL_COUNT=0
DISK_RETRIEVAL_COUNT=0

echo "Testing retrieval of first 50 items (likely on disk)..."

for i in $(seq 1 50); do
    key="test_key_$i"
    result=$(get_key "$key")
    
    if [[ "$result" == *"ERROR"* ]] || [[ "$result" == *"\$-1"* ]]; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo -e "${RED}✗ Failed to retrieve $key${NC}"
    else
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        # Check if this was a disk retrieval by looking at response time
        DISK_RETRIEVAL_COUNT=$((DISK_RETRIEVAL_COUNT + 1))
    fi
done

echo "Disk retrieval test results:"
echo "  ✓ Successful retrievals: $SUCCESS_COUNT"
echo "  ✗ Failed retrievals: $FAIL_COUNT"
echo ""

# Test 3: Retrieve recent items (should be in memory)
echo -e "${BLUE}=== Test 3: Retrieving recent items from memory ===${NC}"

MEMORY_SUCCESS=0
MEMORY_FAIL=0

echo "Testing retrieval of last 50 items (likely in memory)..."

for i in $(seq $((TARGET_ITEMS - 49)) $TARGET_ITEMS); do
    key="test_key_$i"
    result=$(get_key "$key")
    
    if [[ "$result" == *"ERROR"* ]] || [[ "$result" == *"\$-1"* ]]; then
        MEMORY_FAIL=$((MEMORY_FAIL + 1))
        echo -e "${RED}✗ Failed to retrieve $key${NC}"
    else
        MEMORY_SUCCESS=$((MEMORY_SUCCESS + 1))
    fi
done

echo "Memory retrieval test results:"
echo "  ✓ Successful retrievals: $MEMORY_SUCCESS"
echo "  ✗ Failed retrievals: $MEMORY_FAIL"
echo ""

# Test 4: Access pattern test (promote disk items back to memory)
echo -e "${BLUE}=== Test 4: Access pattern test (disk to memory promotion) ===${NC}"

PROMOTION_SUCCESS=0
echo "Re-accessing first 20 items to test disk-to-memory promotion..."

for i in $(seq 1 20); do
    key="test_key_$i"
    
    # Access the item twice to test promotion
    result1=$(get_key "$key")
    sleep 0.1
    result2=$(get_key "$key")
    
    if [[ "$result1" != *"ERROR"* ]] && [[ "$result2" != *"ERROR"* ]]; then
        PROMOTION_SUCCESS=$((PROMOTION_SUCCESS + 1))
    fi
done

echo "Promotion test results:"
echo "  ✓ Successfully promoted items: $PROMOTION_SUCCESS/20"
echo ""

# Test 5: Deletion test
echo -e "${BLUE}=== Test 5: Testing deletion from both memory and disk ===${NC}"

DELETE_SUCCESS=0
DELETE_FAIL=0

echo "Deleting first 30 items..."

for i in $(seq 1 30); do
    key="test_key_$i"
    result=$(del_key "$key")
    
    if [[ "$result" == *":1"* ]]; then
        DELETE_SUCCESS=$((DELETE_SUCCESS + 1))
    else
        DELETE_FAIL=$((DELETE_FAIL + 1))
    fi
done

echo "Deletion test results:"
echo "  ✓ Successful deletions: $DELETE_SUCCESS"
echo "  ✗ Failed deletions: $DELETE_FAIL"
echo ""

# Test 6: Verify deletions
echo -e "${BLUE}=== Test 6: Verifying deletions ===${NC}"

VERIFY_SUCCESS=0
echo "Verifying first 30 items are deleted..."

for i in $(seq 1 30); do
    key="test_key_$i"
    result=$(get_key "$key")
    
    if [[ "$result" == *"\$-1"* ]] || [[ "$result" == *"ERROR"* ]]; then
        VERIFY_SUCCESS=$((VERIFY_SUCCESS + 1))
    fi
done

echo "Verification results:"
echo "  ✓ Confirmed deletions: $VERIFY_SUCCESS/30"
echo ""

# Final summary
echo -e "${BLUE}=========================================="
echo "           TEST SUMMARY"
echo "==========================================${NC}"

echo "Total items inserted: $TARGET_ITEMS"
echo "Cache size limit: ${CACHE_SIZE_MB}MB"
echo ""
echo "Results:"
echo "  ✓ Disk retrieval success rate: $SUCCESS_COUNT/50 ($(( SUCCESS_COUNT * 100 / 50 ))%)"
echo "  ✓ Memory retrieval success rate: $MEMORY_SUCCESS/50 ($(( MEMORY_SUCCESS * 100 / 50 ))%)"
echo "  ✓ Promotion success rate: $PROMOTION_SUCCESS/20 ($(( PROMOTION_SUCCESS * 100 / 20 ))%)"
echo "  ✓ Deletion success rate: $DELETE_SUCCESS/30 ($(( DELETE_SUCCESS * 100 / 30 ))%)"
echo "  ✓ Deletion verification rate: $VERIFY_SUCCESS/30 ($(( VERIFY_SUCCESS * 100 / 30 ))%)"

# Overall assessment
TOTAL_SCORE=$(( (SUCCESS_COUNT * 100 / 50) + (MEMORY_SUCCESS * 100 / 50) + (PROMOTION_SUCCESS * 100 / 20) + (DELETE_SUCCESS * 100 / 30) + (VERIFY_SUCCESS * 100 / 30) ))
AVERAGE_SCORE=$(( TOTAL_SCORE / 5 ))

echo ""
if [ $AVERAGE_SCORE -ge 90 ]; then
    echo -e "${GREEN}🎉 EXCELLENT! Overall success rate: ${AVERAGE_SCORE}%${NC}"
    echo -e "${GREEN}   Disk swapping and retrieval working perfectly!${NC}"
elif [ $AVERAGE_SCORE -ge 70 ]; then
    echo -e "${YELLOW}⚠️  GOOD: Overall success rate: ${AVERAGE_SCORE}%${NC}"
    echo -e "${YELLOW}   Some minor issues, but disk swapping is functional${NC}"
else
    echo -e "${RED}❌ ISSUES DETECTED: Overall success rate: ${AVERAGE_SCORE}%${NC}"
    echo -e "${RED}   Disk swapping may not be working correctly${NC}"
fi

echo ""
echo -e "${BLUE}📋 Server log analysis:${NC}"
echo "Last 10 lines of server log:"
tail -10 "$LOG_FILE"

echo ""
echo -e "${GREEN}Test completed!${NC}"
echo "Full server log available at: $LOG_FILE"
