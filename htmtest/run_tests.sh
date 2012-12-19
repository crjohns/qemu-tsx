#!/bin/bash

for i in `seq 1 32`; do make -C qemu/htmtest/ test && make -C xv6/ qemu QEMUEXTRA='-cpu Haswell -no-reboot' CPUS=$i; ./qemu/htmtest/parselog.py ./xv6/execlog > results/cpu-$i-results.txt; done
