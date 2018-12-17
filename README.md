# lambda_nic
Set of Experiments for Lambda NIC project


## Things to do when you restart.

```
rmmod nfp
insmod nfp nfp_dev_cpp=1
nfp-nffw unload -n 0
systemctl start nfp-sdk6-rte
/opt/netronome/p4/bin/rtecli design-unload
/opt/netronome/p4/bin/rtecli design-load -f AgilioCX.nffw -p out/pif_design.json -c simple_server.p4cfg
lambda_nic/tools/run_ethtool.sh vf0_1
```

## To build a P4/Sandbox C program from Linux.

First go to the folder where the `p4` and the `c` file is located.
```
nfp4build -p ./out/ -o AgilioCX.nffw -l hydrogen -4 <filename>.p4 -c <filename>.c
```


## Things to note when you add a machine.

1. Make sure to check `/opt/nfp_pif/scripts/pif_ctl_nfd.sh` and change the base MAC for vfs.

