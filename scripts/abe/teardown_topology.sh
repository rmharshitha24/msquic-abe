#!/bin/bash
for ns in s1 s2 r1 r2 d1 d2; do ip netns del $ns 2>/dev/null || true; done
echo "Topology removed"
