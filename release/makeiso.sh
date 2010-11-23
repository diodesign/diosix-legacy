#!/bin/bash

# build a bootable ISO9600 image file (suitable for CDs) called ./cd.iso out of the filesystem in ./root

CMD="mkisofs -R -quiet -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o release/cd.iso `pwd`/release/root"

$CMD; 
exit 0;
