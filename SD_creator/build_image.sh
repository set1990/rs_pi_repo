#!/bin/bash
set -e

# ---------- PARAMETRY ----------
while [[ $# -gt 0 ]]; do
  case $1 in
    --ssid) SSID="$2"; shift 2;;
    --psk) PSK="$2"; shift 2;;
    --hostname) HOSTNAME="$2"; shift 2;;
    --sshkey) SSHKEY_FILE="$2"; shift 2;;
    --img-size) IMG_SIZE="$2"; shift 2;;
    *) echo "Nieznany parametr $1"; exit 1;;
  esac
done

if [ -z "$SSID" ] || [ -z "$PSK" ] || [ -z "$HOSTNAME" ] || [ -z "$SSHKEY_FILE" ]; then
  echo "Brak wymaganych parametrów"
  exit 1
fi

IMG_SIZE=${IMG_SIZE:-2048}
IMG=arch-${HOSTNAME}.img

SSHKEY=$(cat $SSHKEY_FILE)

echo "Tworzenie obrazu ${IMG_SIZE}MB..."
dd if=/dev/zero of=$IMG bs=1M count=$IMG_SIZE

LOOP=$(losetup -f --show $IMG)
parted -s $LOOP mklabel msdos
parted -s $LOOP mkpart primary fat32 1MiB 200MiB
parted -s $LOOP mkpart primary ext4 200MiB 100%
parted -s $LOOP set 1 boot on

partprobe $LOOP
sleep 2

BOOT=${LOOP}p1
ROOT=${LOOP}p2

mkfs.vfat -F32 $BOOT
mkfs.ext4 -F $ROOT

mkdir -p mnt/boot mnt/root
mount $ROOT mnt/root
mkdir mnt/root/boot
mount $BOOT mnt/boot

echo "Pobieranie Arch Linux ARM..."
wget -O arch.tar.gz \
https://archlinuxarm.org/os/ArchLinuxARM-rpi-armv7-latest.tar.gz

bsdtar -xpf arch.tar.gz -C mnt/root
sync
mv mnt/root/boot/* mnt/boot/

# UART + watchdog
echo "enable_uart=1" >> mnt/boot/config.txt
echo "dtoverlay=disable-bt" >> mnt/boot/config.txt
echo "dtparam=watchdog=on" >> mnt/boot/config.txt

echo "console=serial0,115200 console=tty1 root=/dev/mmcblk0p2 rw rootwait fsck.repair=yes" > mnt/boot/cmdline.txt

# kopiowanie firstrun
cp firstrun.sh mnt/root/root/firstrun.sh
chmod +x mnt/root/root/firstrun.sh

# przekazanie zmiennych
cat > mnt/root/root/firstboot.env <<EOF
SSID="$SSID"
PSK="$PSK"
HOSTNAME="$HOSTNAME"
SSHKEY="$SSHKEY"
EOF

# systemd service
cat > mnt/root/etc/systemd/system/firstrun.service <<EOF
[Unit]
Description=First boot initialization
After=network.target

[Service]
Type=oneshot
ExecStart=/root/firstrun.sh
RemainAfterExit=true

[Install]
WantedBy=multi-user.target
EOF

ln -s /etc/systemd/system/firstrun.service \
mnt/root/etc/systemd/system/multi-user.target.wants/firstrun.service

umount mnt/boot
umount mnt/root
losetup -d $LOOP
rm -rf mnt arch.tar.gz

echo "Gotowe: $IMG"
