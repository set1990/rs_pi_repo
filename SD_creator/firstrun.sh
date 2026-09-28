#!/bin/bash

echo "=== FIRST RUN START =========="
# 1️. Wczytanie zmiennych przekazanych przez build_image.sh
set -e
source /root/firstboot.env

# 2️. Tworzymy plik wpa_supplicant zanim włączymy sieć
echo "=== SSID config =============="
cat > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf <<EOF
ctrl_interface=DIR=/run/wpa_supplicant
update_config=1
country=PL

network={
    ssid="$SSID"
    psk="$PSK"
}
EOF
chmod 600 /etc/wpa_supplicant/wpa_supplicant-wlan0.conf

echo "=== WPA config ==============="
cat > /etc/systemd/system/wifi-autostart.service <<EOF
[Unit]
Description=WiFi wext
After=network.target

[Service]
Type=forking
ExecStartPre=/usr/bin/ip link set wlan0 up
ExecStartPre=/usr/bin/iw dev wlan0 set power_save off
ExecStart=/usr/bin/wpa_supplicant -B -Dwext -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
ExecStartPost=/usr/bin/dhcpcd wlan0

[Install]
WantedBy=multi-user.target
EOF
chmod 644 /etc/systemd/system/wifi-autostart.service 

# 3️. Włączamy sieć
echo "=== Wi-fi enable ============="
systemctl enable --now wifi-autostart.service

# 4️. Czekamy na przydzielenie IP
echo "=== IP enable ================"
echo "Czekam na IP dla wlan0..."
for i in {1..30}; do
    IP=$(ip -4 addr show wlan0 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
    if [ -n "$IP" ]; then
        echo "IP przydzielone: $IP"
        break
    fi
    sleep 2
done

if [ -z "$IP" ]; then
    echo "Nie udało się uzyskać IP, wychodzę"
    exit 1
fi

# 5️. Inicjalizacja pacmana i aktualizacja systemu
echo "=== pacman init =============="
echo "Inicjalizacja pacmana..."
pacman-key --init
pacman-key --populate archlinuxarm

echo "=== system update ============"
pacman -Syu --noconfirm
pacman -S --noconfirm openssh htop avahi nss-mdns rsync gdb

# 6. Hostname i /etc/hosts
echo "=== HOSTNAME set ============="
echo "$HOSTNAME" > /etc/hostname
cat > /etc/hosts <<EOF
127.0.0.1   localhost
::1         localhost
127.0.1.1   ${HOSTNAME}.local ${HOSTNAME}
EOF

sed -i 's/hosts: mymachines resolve [!UNAVAIL=return] files myhostname dns/hosts: files mymachines mdns_minimal [NOTFOUND=return] resolve [!UNAVAIL=return] myhostname dns/' /etc/nsswitch.conf
sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

# 7. Włączenie usług systemowych
echo "=== enable services =========="
systemctl enable sshd
systemctl enable avahi-daemon

# 8. Konfiguracja SSH (klucz)
echo "=== root key create =========="
mkdir -p /root/.ssh
echo "$SSHKEY" > /root/.ssh/authorized_keys
chmod 700 /root/.ssh
chmod 600 /root/.ssh/authorized_keys
chown -R root:root /root/.ssh

# 9. Dezaktywacja firstrun
echo "=== clean service ============"
systemctl disable firstrun.service
rm -f /etc/systemd/system/firstrun.service
rm -f /root/firstboot.env
rm -f /root/firstrun.sh
echo "=== FIRST RUN DONE ==="
reboot 