ifdown $1
ifup $1

#!/bin/bash
/sbin/ethtool --coalesce $1 rx-usecs 1
/sbin/ethtool --coalesce $1 tx-usecs 1
/sbin/ethtool --coalesce $1 rx-frames 1
/sbin/ethtool --coalesce $1 tx-frames 1

# Adding route
ip route del 20.20.0.0/16
ip route del 20.20.20.0/24
ip route add 30.30.30.105/32 dev $1
ip route add 20.20.0.0/16 dev $1

ethtool -K $1 tx off rx off
