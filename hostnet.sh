sudo ip tuntap add dev tap0 mode tap user `whoami`
sudo ip a a dev tap0 192.168.13.1/24
sudo ip link set tap0 up

# The following lines are needed if you plan to access Internet
sudo sysctl net.ipv4.ip_forward=1
sudo sysctl net.ipv6.conf.default.forwarding=1
sudo sysctl net.ipv6.conf.all.forwarding=1

export INTERNET_IF=enp1s0

sudo iptables -t nat -A POSTROUTING -o $INTERNET_IF -j MASQUERADE
sudo iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i tap0 -o $INTERNET_IF -j ACCEPT
