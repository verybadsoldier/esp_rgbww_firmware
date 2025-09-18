#!/bin/bash
# filepath: /home/pjakobs/devel/esp_rgbww_firmware/test_tap.sh

TAP_IF="tap0"

if ! ip link show "$TAP_IF" &>/dev/null; then
    echo "Interface $TAP_IF does not exist."
    exit 1
fi

STATE=$(ip link show "$TAP_IF" | awk '/state/ {print $9}')
echo "Interface $TAP_IF state: $STATE"

IP=$(ip addr show "$TAP_IF" | awk '/inet / {print $2}')
if [ -n "$IP" ]; then
    echo "IP address: $IP"
else
    echo "No IP address assigned to $TAP_IF."
fi