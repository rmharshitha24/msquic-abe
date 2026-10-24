#!/bin/bash
# Usage: sudo ./setup_topology.sh [ecn]    (add "ecn" to mark packets instead of dropping)
set -e
ECN=${1:-noecn}
RATE=10mbit; DELAY=20ms

# clean any old topology
for ns in s1 s2 r1 r2 d1 d2; do ip netns del $ns 2>/dev/null || true; done
for ns in s1 s2 r1 r2 d1 d2; do ip netns add $ns; done

link() { # nsA devA ipA nsB devB ipB
  ip link add $2 type veth peer name $5
  ip link set $2 netns $1; ip link set $5 netns $4
  ip -n $1 addr add $3/24 dev $2; ip -n $4 addr add $6/24 dev $5
  ip -n $1 link set $2 up;        ip -n $4 link set $5 up
  ip netns exec $1 ethtool -K $2 tso off gso off gro off >/dev/null 2>&1 || true
  ip netns exec $4 ethtool -K $5 tso off gso off gro off >/dev/null 2>&1 || true
}
link s1 s1-r1 10.0.1.1 r1 r1-s1 10.0.1.2
link s2 s2-r1 10.0.2.1 r1 r1-s2 10.0.2.2
link r1 r1-r2 10.0.3.1 r2 r2-r1 10.0.3.2     # bottleneck link
link r2 r2-d1 10.0.4.1 d1 d1-r2 10.0.4.2
link r2 r2-d2 10.0.5.1 d2 d2-r2 10.0.5.2

for ns in s1 s2 r1 r2 d1 d2; do ip -n $ns link set lo up; done
ip netns exec r1 sysctl -qw net.ipv4.ip_forward=1
ip netns exec r2 sysctl -qw net.ipv4.ip_forward=1

# routes
ip -n s1 route add default via 10.0.1.2
ip -n s2 route add default via 10.0.2.2
ip -n d1 route add default via 10.0.4.1
ip -n d2 route add default via 10.0.5.1
ip -n r1 route add 10.0.4.0/24 via 10.0.3.2
ip -n r1 route add 10.0.5.0/24 via 10.0.3.2
ip -n r2 route add 10.0.1.0/24 via 10.0.3.1
ip -n r2 route add 10.0.2.0/24 via 10.0.3.1

# bottleneck: rate limit + queue on R1's outgoing side
ip netns exec r1 tc qdisc add dev r1-r2 root handle 1: htb default 10
ip netns exec r1 tc class add dev r1-r2 parent 1: classid 1:10 htb rate $RATE
if [ "$ECN" = "ecn" ]; then
  ip netns exec r1 tc qdisc add dev r1-r2 parent 1:10 red limit 300000 avpkt 1200 \
     min 20000 max 60000 probability 0.05 bandwidth $RATE ecn
else
  ip netns exec r1 tc qdisc add dev r1-r2 parent 1:10 pfifo limit 100
fi
# propagation delay on the return path (sets the RTT)
ip netns exec r2 tc qdisc add dev r2-r1 root netem delay $DELAY
echo "Topology ready (mode: $ECN)"
