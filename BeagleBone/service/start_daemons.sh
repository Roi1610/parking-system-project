#!/bin/bash

CLIENT_DAEMON="/home/debian/embedded/CLIENT_DAEMON"
I2C_DAEMON="/home/debian/embedded/I2C_DAEMON"

# Start CLIENT_DAEMON first
if [ -x "$CLIENT_DAEMON" ]; then
    echo "Starting CLIENT_DAEMON..."
    $CLIENT_DAEMON &
else
    echo "Error: CLIENT_DAEMON not found or not executable"
    exit 1
fi

# Wait until CLIENT_DAEMON is running
for i in {1..3}; do
    if pgrep -x "CLIENT_DAEMON" > /dev/null; then
        echo "CLIENT_DAEMON is running"
        break
    fi
    echo "Waiting for CLIENT_DAEMON..."
    sleep 1
done

# Start I2C_DAEMON only after client is alive
if [ -x "$I2C_DAEMON" ]; then
    echo "Starting I2C_DAEMON..."
    $I2C_DAEMON &
else
    echo "Error: I2C_DAEMON not found or not executable"
    exit 1
fi

exit 0
