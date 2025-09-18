#!/bin/bash

set -e

TAP_IF="tap0"
BRIDGE_IF="br0"

usage() {
    echo "Usage: $0 {start|stop} <external_interface>"
    exit 1
}

if [ "$#" -lt 2 ]; then
    usage
fi

CMD="$1"
EXT_IF="$2"

get_ip() {
    ip addr show "$1" | awk '/inet / {print $2}'
}

get_gw() {
    ip route show default | grep "dev $EXT_IF" | awk '{print $3}'
}

start() {
    echo "Creating TAP interface $TAP_IF..."
    sudo ip tuntap add dev $TAP_IF mode tap user $(whoami)
    sudo ip link set $TAP_IF up

    echo "Creating bridge $BRIDGE_IF and adding $TAP_IF and $EXT_IF..."
    sudo ip link add name $BRIDGE_IF type bridge || true
    sudo ip link set $TAP_IF master $BRIDGE_IF
    sudo ip link set $EXT_IF master $BRIDGE_IF
    sudo ip link set $BRIDGE_IF up

    sudo ip link set $TAP_IF up

    # Save current IP config
    HOST_IP=$(get_ip $EXT_IF)
    HOST_GW=$(get_gw)
    echo "Saving host IP: $HOST_IP, GW: $HOST_GW"
    echo "$HOST_IP" > /tmp/hostnet_ip
    echo "$HOST_GW" > /tmp/hostnet_gw

    echo "Flushing IP from $EXT_IF and assigning to $BRIDGE_IF..."
    sudo ip addr flush dev $EXT_IF
    if [ -n "$HOST_IP" ]; then
        sudo ip addr add $HOST_IP dev $BRIDGE_IF
    else
        echo "No static IP found, trying DHCP on $BRIDGE_IF..."
        sudo dhclient $BRIDGE_IF
    fi

    # Remove default route from EXT_IF and add to BRIDGE_IF
    if [ -n "$HOST_GW" ]; then
        sudo ip route del default || true
        sudo ip route add default via $HOST_GW dev $BRIDGE_IF
    fi

    echo "Enabling IP forwarding..."
    sudo sysctl -w net.ipv4.ip_forward=1
    sudo sysctl -w net.ipv6.conf.default.forwarding=1
    sudo sysctl -w net.ipv6.conf.all.forwarding=1

    echo "Configuring firewall..."
    sudo firewall-cmd --permanent --zone=public --add-masquerade
    sudo firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 -i $BRIDGE_IF -o $EXT_IF -j ACCEPT
    sudo firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 -i $EXT_IF -o $BRIDGE_IF -j ACCEPT
    sudo firewall-cmd --reload

    echo "Network setup complete."
}

stop() {
    echo "Removing firewall rules..."
    sudo firewall-cmd --permanent --direct --remove-rule ipv4 filter FORWARD 0 -i $BRIDGE_IF -o $EXT_IF -j ACCEPT || true
    sudo firewall-cmd --permanent --direct --remove-rule ipv4 filter FORWARD 0 -i $EXT_IF -o $BRIDGE_IF -j ACCEPT || true
    sudo firewall-cmd --permanent --zone=public --remove-masquerade || true
    sudo firewall-cmd --reload

    echo "Restoring IP to $EXT_IF..."
    HOST_IP=""
    HOST_GW=""
    # Remove interfaces from bridge before IP changes
    sudo ip link set $EXT_IF nomaster || true
    sudo ip link set $TAP_IF nomaster || true

    # Flush IPs from both interfaces
    sudo ip addr flush dev $BRIDGE_IF || true
    sudo ip addr flush dev $EXT_IF || true

    echo "Deleting bridge $BRIDGE_IF..."
    sudo ip link set $BRIDGE_IF down || true
    sudo ip link delete $BRIDGE_IF type bridge || true

    if [ -f /tmp/hostnet_ip ]; then
        HOST_IP=$(cat /tmp/hostnet_ip)
        if [ -n "$HOST_IP" ]; then
            # Only add IP if not already present
            if ! ip addr show $EXT_IF | grep -q "$HOST_IP"; then
                sudo ip addr add $HOST_IP dev $EXT_IF
            else
                echo "IP $HOST_IP already present on $EXT_IF, skipping add."
            fi
        else
            echo "No static IP found, trying DHCP on $EXT_IF..."
            sudo dhclient $EXT_IF
        fi
        rm -f /tmp/hostnet_ip
    else
        echo "No saved IP config found, trying DHCP on $EXT_IF..."
        sudo dhclient $EXT_IF
    fi

    if [ -f /tmp/hostnet_gw ]; then
        HOST_GW=$(cat /tmp/hostnet_gw)
        sudo ip route del default || true
        if [ -n "$HOST_GW" ]; then
            sudo ip route add default via $HOST_GW dev $EXT_IF
        fi
        rm -f /tmp/hostnet_gw
    fi

    echo "Deleting TAP interface $TAP_IF..."
    sudo ip link set $TAP_IF down || true
    sudo ip tuntap del dev $TAP_IF mode tap || true

    echo "Network teardown complete."
}

case "$CMD" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    *)
        usage
        ;;
esac