#! /bin/sh

BLCONF="/etc/modprobe.d/blacklist-rtl8xxxu.conf"
MODCONF="/etc/modprobe.d/rtl8xxxu_git.conf"

echo "blacklist rtl8xxxu" > ${BLCONF}
echo "options rtl8xxxu_git ht40_2g=1" > ${MODCONF}

exit 0
