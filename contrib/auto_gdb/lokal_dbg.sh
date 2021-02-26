#!/bin/bash
# use testnet settings,  if you need mainnet,  use ~/.lokalcore/lokald.pid file instead
lokal_pid=$(<~/.lokalcore/testnet3/lokald.pid)
sudo gdb -batch -ex "source debug.gdb" lokald ${lokal_pid}
