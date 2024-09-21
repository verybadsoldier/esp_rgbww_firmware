#!/bin/bash

# Create the TAP interface
sudo ip tuntap add dev tap0 mode tap user `whoami`
sudo ip a add dev tap0 192.168.13.1/24
sudo ip link set tap0 up

# Enable IP forwarding (IPv4 and IPv6)
sudo sysctl net.ipv4.ip_forward=1
sudo sysctl net.ipv6.conf.default.forwarding=1
sudo sysctl net.ipv6.conf.all.forwarding=1

# Set the external network interface
export INTERNET_IF=enp1s0

# Configure firewall-cmd instead of iptables

# Add masquerading (NAT) to allow outgoing traffic from tap0 to the external interface
sudo firewall-cmd --permanent --zone=public --add-masquerade

# Allow forwarding of established connections
#sudo firewall-cmd --permanent --add-rich-rule="rule family=ipv4 forward-port port=0-65535 protocol=tcp to-port=0-65535 to-addr=192.168.13.1"

# Allow traffic forwarding from tap0 to the external interface (INTERNET_IF)
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 -i enp2s0 -o tap0  -j ACCEPT
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 -i tap0 -o enp2s0  -j ACCEPT


# Apply the firewall changes
sudo firewall-cmd --reload

echo "TAP interface tap0 created and traffic forwarding configured."

