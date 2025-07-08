#!/bin/bash

# Demonstration of arena allocator automatic cleanup in Modbus server/client

echo "=== Protocol Toolkit Arena Allocator Demonstration ==="
echo ""
echo "This demo shows the new arena allocator providing automatic cleanup for:"
echo "- Server state and all resources (mutexes, register arrays, buffers)"
echo "- Client allocations and temporary objects"
echo "- No manual memory management required!"
echo ""

cd build

echo "1. Starting Modbus server with arena allocator (64MB arena)..."
./bin/modbus_server --listen-addr=127.0.0.1:5020 --num-holding-regs=10 > server.log 2>&1 &
SERVER_PID=$!
echo "   Server PID: $SERVER_PID"

sleep 2

echo ""
echo "2. Testing client operations with arena allocator (1MB arena)..."
echo ""

echo "   Reading initial value from holding register 1000:"
./bin/modbus_client --host=127.0.0.1 --port=5020 --read-holding=1000 --verbose

echo ""
echo "   Writing value 42 to holding register 1000:"
./bin/modbus_client --host=127.0.0.1 --port=5020 --write-holding=1000,42 --verbose

echo ""
echo "   Reading updated value from holding register 1000:"
./bin/modbus_client --host=127.0.0.1 --port=5020 --read-holding=1000 --verbose

echo ""
echo "   Reading coil 5:"
./bin/modbus_client --host=127.0.0.1 --port=5020 --read-coils=5 --verbose

echo ""
echo "3. Stopping server (triggering automatic cleanup)..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "=== Server log showing arena allocator automatic cleanup ==="
cat server.log

echo ""
echo "=== Demo Complete ==="
echo ""
echo "Key improvements shown:"
echo "✓ Simple arena allocator creation: allocator_arena_create(size, alignment)"
echo "✓ All resources allocated from arena are automatically cleaned up"
echo "✓ No manual ptk_free() calls needed for individual resources"
echo "✓ Simplified error handling - just destroy allocator on exit"
echo "✓ Much safer and more embedded-friendly memory management"

rm -f server.log
