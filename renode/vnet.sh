#!/bin/bash

echo "Creating and setting up the bridge"
# Create the bridge device
ip link add name renode-br0 type bridge

# Set the IP and netmask for the bridge
ip addr add 192.168.67.1/24 brd + dev renode-br0
ip link set renode-br0 up

echo "Creating the tap interface"
# Create tap interface
ip tuntap add tap0 mode tap

# Connect bridge and tap0
ip link set tap0 master renode-br0
ip link set tap0 up

echo "Configuring firewall"
# Enable firewall package routing
ufw allow in on renode-br0
ufw allow out on renode-br0

cleanup() {
  echo ""
  echo "Cleaning up"
  # Undo the firewall rules
  echo "Removing firewall rules"
  ufw delete allow in on renode-br0
  ufw delete allow out on renode-br0

  echo "Removing bridge and shutting down tap"
  # Remove the bridge and shutdown tap
  ip link delete renode-br0
  ip link set tap0 down

  echo "Cleanup completed"
}

# Setting up signal handling
trap "exit" INT TERM ERR
trap "cleanup; kill 0" EXIT

# Starting DHCP Server
echo "************************"
echo "* Starting DHCP Server *"
echo "* Stopping with Ctrl-C *"
echo "************************"

dnsmasq --log-queries \
        --no-hosts \
        --no-resolv \
        --leasefile-ro \
        -d \
        --interface=renode-br0 \
        -p0 --log-dhcp \
        --dhcp-range=192.168.67.2,192.168.67.10



