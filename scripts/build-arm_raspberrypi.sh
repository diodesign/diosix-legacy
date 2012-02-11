#!/bin/bash
set -e

# sync source to build server
cd ..
./rsync-src.sh
cd trunk

# compile any utilities needed using the ARM port's mkpayload
if [ "release/arm_versatilepb/mkpayload.c" -nt "release/arm_versatilepb/mkpayload" ]; then
   echo '[+] building mkpayload utility'
   /usr/bin/gcc -o release/arm_versatilepb/mkpayload release/arm_versatilepb/mkpayload.c
fi

# set up the cross compiler on the Linux build server
# executable paths
export XCOMPILER=/home/chris/x-tools/arm-unknown-eabi
export PREFIX=$XCOMPILER/arm-unknown-eabi/bin/
export PATH=$PREFIX:$XCOMPILER/libexec/gcc/arm-unknown-eabi/4.5.2/:$PATH

# library paths
export C_INCLUDE_PATH=$XCOMPILER/arm-unknown-eabi/include
export C_INCLUDE_PATH=$C_INCLUDE_PATH:$XCOMPILER/lib/gcc/arm-unknown-eabi/4.5.2/include
export LD_INCLUDE_LIBGCC=$XCOMPILER/lib/gcc/arm-unknown-eabi/4.5.2/

# build the kernel and support programs
make -f makefile-arm_raspberrypi
