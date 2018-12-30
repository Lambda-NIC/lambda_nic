ifdown $1
ifup $1

#!/bin/bash
/sbin/ethtool --coalesce $1 rx-usecs 1
/sbin/ethtool --coalesce $1 tx-usecs 1
/sbin/ethtool --coalesce $1 rx-frames 1
/sbin/ethtool --coalesce $1 tx-frames 1

# Adding route
ip route del 10.10.0.0/16
ip route del 10.10.20.0/24
ip route del 10.10.20.105/32
ip route add 10.10.20.105/32 dev $1
ip route add 10.10.0.0/16 dev $1

ethtool -K $1 tx off rx off
