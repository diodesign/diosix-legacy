#!/bin/bash

# build a bootable ISO9600 image file (suitable for CDs) called ./cd.iso out of the filesystem in ./root

cp `pwd`/release/stage2_eltorito `pwd`/release/root/boot/grub/stage2_eltorito
mkisofs -R -quiet -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o release/cd.iso `pwd`/release/root
exit 0;
