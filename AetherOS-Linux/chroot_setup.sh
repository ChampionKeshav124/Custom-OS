#!/bin/bash
# ==============================================================
# AETHEROS LINUX — CHROOT SETUP SCRIPT
# This script runs inside the chroot environment to configure the system.
# ==============================================================

set -e

MODE="all"
if [ "$1" = "--base" ]; then
    MODE="base"
elif [ "$1" = "--config" ]; then
    MODE="config"
fi

export HOME=/root
export DEBIAN_FRONTEND=noninteractive
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

if [ "$MODE" = "all" ] || [ "$MODE" = "base" ]; then
echo "==> Configuring system repositories..."
cat <<EOF > /etc/apt/sources.list
deb http://archive.ubuntu.com/ubuntu/ jammy main restricted universe multiverse
deb http://archive.ubuntu.com/ubuntu/ jammy-updates main restricted universe multiverse
deb http://archive.ubuntu.com/ubuntu/ jammy-security main restricted universe multiverse
EOF

# Disable translation downloads to speed up apt-get update
echo 'Acquire::Languages "none";' > /etc/apt/apt.conf.d/99translations

echo "==> Updating package cache..."
apt-get update

echo "==> Disabling background updates to prevent package manager locking..."
systemctl disable apt-daily.timer || true
systemctl disable apt-daily-upgrade.timer || true
systemctl disable unattended-upgrades.service || true

echo "==> Installing base system, kernel, Caspers, and Live CD boot utilities..."
apt-get install -y --no-install-recommends \
    ubuntu-standard \
    linux-image-virtual \
    linux-headers-virtual \
    casper \
    systemd-sysv

echo "==> Installing display server, video/input drivers, window manager, and login greeter..."
apt-get install -y --no-install-recommends \
    xserver-xorg \
    xserver-xorg-video-all \
    xserver-xorg-video-vesa \
    xserver-xorg-video-fbdev \
    xserver-xorg-video-vmware \
    xserver-xorg-input-all \
    virtualbox-guest-x11 \
    virtualbox-guest-utils \
    xinit \
    openbox \
    pcmanfm \
    mousepad \
    lxterminal \
    lxtask \
    galculator \
    lxappearance \
    obconf \
    lxappearance-obconf \
    lxrandr \
    pavucontrol \
    arc-theme \
    papirus-icon-theme \
    greybird-gtk-theme \
    clearlooks-phenix-theme \
    fonts-liberation \
    libatspi2.0-0 \
    libgtk-3-0 \
    libnspr4 \
    libvulkan1 \
    xdg-utils \
    libcurl4 \
    feh \
    dbus-x11 \
    lightdm \
    lightdm-gtk-greeter \
    numlockx \
    pulseaudio \
    alsa-utils \
    network-manager \
    librsvg2-common \
    librsvg2-bin \
    drawing \
    xpad \
    osmo \
    vlc \
    ristretto \
    file-roller \
    p7zip-full


echo "==> Removing static xorg.conf to allow dynamic Xorg auto-detection..."
rm -f /etc/X11/xorg.conf

echo "==> Enabling LightDM as default display manager..."
systemctl enable lightdm || true
ln -sf /lib/systemd/system/lightdm.service /etc/systemd/system/display-manager.service || true

# Add a 10-second delay to LightDM startup to allow the Plymouth boot splash to show longer
mkdir -p /etc/systemd/system/lightdm.service.d
cat <<'EOF' > /etc/systemd/system/lightdm.service.d/delay.conf
[Service]
ExecStartPre=/bin/sleep 10
EOF

echo "==> Creating Openbox session desktop entry..."
mkdir -p /usr/share/xsessions
cat <<'EOF' > /usr/share/xsessions/openbox.desktop
[Desktop Entry]
Name=Openbox
Comment=Log in using the Openbox window manager (without a session manager)
Exec=/usr/bin/openbox-session
TryExec=/usr/bin/openbox-session
Icon=openbox
Type=Application
EOF

echo "==> Installing Node.js, Python, and Chrome GUI dependencies..."
apt-get install -y --no-install-recommends curl ca-certificates gnupg
curl -fsSL https://deb.nodesource.com/setup_18.x | bash -
apt-get install -y --no-install-recommends \
    nodejs \
    python3 \
    python3-pip \
    python3-venv \
    python3-tk \
    libnss3 \
    libatk1.0-0 \
    libatk-bridge2.0-0 \
    libcups2 \
    libdrm2 \
    libxkbcommon0 \
    libxcomposite1 \
    libxdamage1 \
    libxrandr2 \
    libgbm1 \
    libpango-1.0-0 \
    libcairo2 \
    libasound2 \
    libxshmfence1 \
    libglu1-mesa \
    libxss1 \
    libxtst6 \
    libpci3 \
    portaudio19-dev \
    libgl1-mesa-glx \
    libglib2.0-0 \
    python3-all-dev \
    x11-xserver-utils \
    sudo \
    wget \
    curl \
    git \
    zenity \
    xdotool \
    xclip \
    xsel

echo "==> Installing Plymouth for custom boot screen..."
apt-get install -y --no-install-recommends plymouth plymouth-themes

echo "==> Installing Copilot dependencies..."
pip3 install --timeout 60 --no-cache-dir customtkinter requests || \
pip3 install --timeout 60 --no-cache-dir -i https://pypi.tuna.tsinghua.edu.cn/simple customtkinter requests
fi

if [ "$MODE" = "all" ] || [ "$MODE" = "config" ]; then
echo "==> Creating custom Aether user..."
if ! id -u aether >/dev/null 2>&1; then
    useradd -m -s /bin/bash aether
fi
passwd -d aether
echo "aether ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Create autologin and nopasswdlogin groups for LightDM autologin support
groupadd -r autologin || true
groupadd -r nopasswdlogin || true

# Add aether to vboxsf group so VirtualBox Shared Folders auto-mount and are readable
groupadd vboxsf 2>/dev/null || true
usermod -aG audio,video,sudo,cdrom,autologin,nopasswdlogin,vboxsf aether

# Create the shared folder mount point that setup_vm.ps1 maps to
mkdir -p /media/sf_AetherOS
chown aether:vboxsf /media/sf_AetherOS
chmod 770 /media/sf_AetherOS

echo "==> Setting default systemd target to graphical (GUI boot)..."
systemctl set-default graphical.target || ln -sf /lib/systemd/system/graphical.target /etc/systemd/system/default.target

echo "==> Configuring Netplan to use NetworkManager..."
mkdir -p /etc/netplan
cat <<'NETEOF' > /etc/netplan/01-network-manager-all.yaml
network:
  version: 2
  renderer: NetworkManager
NETEOF

echo "==> Enabling NetworkManager service..."
systemctl enable NetworkManager || true

echo "==> Configuring PAM for LightDM passwordless autologin..."
mkdir -p /etc/pam.d
cat <<'PAMEOF' > /etc/pam.d/lightdm-autologin
auth        required    pam_env.so
auth        required    pam_permit.so
account     include     lightdm
password    include     lightdm
session     required    pam_limits.so
session     required    pam_env.so
session     required    pam_unix.so
session     optional    pam_systemd.so
PAMEOF

echo "==> Configuring LightDM autologin..."
mkdir -p /etc/lightdm
cat <<EOF > /etc/lightdm/lightdm.conf
[LightDM]
logind-check-graphical=false

[Seat:*]
autologin-user=aether
autologin-user-timeout=0
user-session=openbox
greeter-session=lightdm-gtk-greeter
xserver-allow-tcp=false
EOF

echo "==> Setting up desktop launcher folder structures..."
mkdir -p /home/aether/Desktop
mkdir -p /home/aether/Downloads
mkdir -p /home/aether/.config/openbox

echo "==> Copying custom files and scripts into user home directory..."
# The build_iso.sh script will copy Copilot and deb files to chroot staging folder /tmp/staging.
# We move them to their final user home directories.
if [ -d /tmp/staging/copilot ]; then
    echo "==> Copying Copilot app files..."
    rm -rf /home/aether/copilot
    cp -r /tmp/staging/copilot /home/aether/copilot
    if [ -f /tmp/staging/copilot/aether_pilot.png ]; then
        echo "==> Copying Aether Pilot custom icon to system pixmaps..."
        cp /tmp/staging/copilot/aether_pilot.png /usr/share/pixmaps/aether_pilot.png
    fi
fi

if [ -f /tmp/staging/google-chrome.deb ]; then
    echo "==> Preloading Google Chrome deb package..."
    cp /tmp/staging/google-chrome.deb /home/aether/Downloads/google-chrome.deb
    echo "==> Pre-installing Google Chrome and resolving library dependencies..."
    dpkg -i /tmp/staging/google-chrome.deb || apt-get install -y -f
fi

echo "==> Copying application desktop entries..."
cp /tmp/staging/*.desktop /home/aether/Desktop/ || true
chmod +x /home/aether/Desktop/*.desktop || true

if [ -f "/home/aether/copilot/Copilot.desktop" ]; then
    echo "==> Copying Copilot desktop entry..."
    cp "/home/aether/copilot/Copilot.desktop" "/home/aether/Desktop/Copilot.desktop"
    chmod +x "/home/aether/Desktop/Copilot.desktop"
fi

# Make launcher scripts executable
if [ -d /home/aether/copilot/launchers ]; then
    chmod +x /home/aether/copilot/launchers/chrome_setup.sh
    chmod +x /home/aether/copilot/launchers/chrome_setup.py
fi
chmod +x /home/aether/copilot/aetheros_shell.py || true
chmod +x /home/aether/copilot/aetheros-intro.py || true
chmod +x /home/aether/copilot/aether-desktop-sync.py || true
chmod +x /usr/local/bin/aether-install || true
chmod +x /usr/local/bin/aether-installer-gui.py || true


# Ensure correct files permissions
chown -R aether:aether /home/aether


echo "==> Configuring custom Openbox autostart..."
cat <<'EOF' > /home/aether/.config/openbox/autostart
# Start VirtualBox Guest Services (clipboard sync, etc.)
/usr/bin/VBoxClient-all &

# Allow local root connections to X11 (for sudo GUI applications)
xhost +local:root || true

# Start the full-screen futuristic graphical desktop intro animation
python3 /home/aether/copilot/aetheros-intro.py

# Start PCManFM to manage desktop icons and wallpaper background
pcmanfm --desktop &

# Unmute Pulseaudio
pulseaudio --start &
amixer set Master unmute
amixer set Master 100%

# Background daemon loop to automatically discover and mount keys.iso
(
  echo "[KEYS DAEMON] Starting keys mount daemon..."
  for i in {1..20}; do
    echo "[KEYS DAEMON] Scanning SCSI devices (attempt $i)..."
    for dev in /dev/sr*; do
      if [ -b "$dev" ]; then
        # Check if already mounted as boot cdrom
        if grep -q "$dev /cdrom" /proc/mounts; then
          echo "[KEYS DAEMON] Skipping boot device $dev"
          continue
        fi
        dev_name=$(basename "$dev")
        echo "[KEYS DAEMON] Found CD-ROM: $dev. Mounting to /media/aether/$dev_name"
        sudo mkdir -p "/media/aether/$dev_name"
        if sudo mount -o ro "$dev" "/media/aether/$dev_name" 2>/dev/null; then
          echo "[KEYS DAEMON] Mounted successfully. Creating link to /media/aether/KEYS"
          sudo mkdir -p /media/aether/KEYS
          sudo mount --bind "/media/aether/$dev_name" /media/aether/KEYS 2>/dev/null || sudo ln -sf "/media/aether/$dev_name" /media/aether/KEYS
          echo "[KEYS DAEMON] Setup complete!"
          break 2
        fi
      fi
    done
    sleep 1
  done
) > /home/aether/keys_mount.log 2>&1 &

# Start desktop shortcut sync daemon
python3 /home/aether/copilot/aether-desktop-sync.py > /home/aether/desktop_sync.log 2>&1 &

# Start Copilot GUI chatbot on boot (delay slightly to allow VBoxClient to load)
(sleep 2 && python3 -u /home/aether/copilot/copilot.py > /home/aether/copilot.log 2>&1) &
EOF

echo "==> Configuring Openbox keyboard shortcut (Super + C) for Copilot..."
mkdir -p /home/aether/.config/openbox
cp /etc/xdg/openbox/rc.xml /home/aether/.config/openbox/rc.xml
python3 -c "
path = '/home/aether/.config/openbox/rc.xml'
with open(path, 'r') as f:
    content = f.read()
keybind_str = '''  <keybind key=\"W-c\">
      <action name=\"Execute\">
        <command>python3 /home/aether/copilot/copilot.py</command>
      </action>
    </keybind>
  </keyboard>'''
if '</keyboard>' in content:
    content = content.replace('</keyboard>', keybind_str)
app_rule = '''  <application title=\"AETHER PILOT\">
    <decor>no</decor>
    <focus>yes</focus>
    <layer>above</layer>
  </application>
</applications>'''
if '</applications>' in content:
    content = content.replace('</applications>', app_rule)
import re
if '<theme>' in content:
    content = re.sub(r'<theme>\s*<name>[^<]+</name>', '<theme>\\n    <name>Arc-Darker</name>', content)
with open(path, 'w') as f:
    f.write(content)
"
echo "==> Configuring PCManFM default desktop settings..."
mkdir -p /home/aether/.config/pcmanfm/default
cat <<'EOF' > /home/aether/.config/pcmanfm/default/desktop-items-0.conf
[*]
wallpaper_mode=stretch
wallpaper=/usr/share/backgrounds/aether_hologram.png
desktop_bg=#000000
desktop_fg=#ffffff
desktop_shadow=#000000
show_mounts=0
show_trash=0
show_documents=0
show_directory=0
cell_width=130
cell_height=100
icon_size=48
EOF

cat <<'EOF' > /home/aether/.config/pcmanfm/default/pcmanfm.conf
[config]
bm_open_method=0

[volume]
mount_on_startup=1
mount_removable=1
autorun=1

[desktop]
wallpaper_mode=stretch
desktop_bg=#000000
desktop_fg=#ffffff
desktop_shadow=#000000
show_wm_menu=0

[ui]
always_show_tabs=0
max_tab_chars=32
win_width=640
win_height=480
splitter_pos=150
media_in_terminal=0
desktop_folder_new_instance=0
show_hidden=0
theme_name=Arc-Darker
icon_theme_name=Papirus
use_default_theme=0
EOF

echo "==> Configuring single-click behavior for desktop and files..."
mkdir -p /home/aether/.config/libfm
cat <<'EOF' > /home/aether/.config/libfm/libfm.conf
[config]
single_click=1
EOF

echo "==> Configuring default GTK themes and icons..."
mkdir -p /home/aether/.config/gtk-3.0
cat <<'EOF' > /home/aether/.config/gtk-3.0/settings.ini
[Settings]
gtk-theme-name=Arc-Darker
gtk-icon-theme-name=Papirus
gtk-font-name=Ubuntu 11
gtk-cursor-theme-name=Adwaita
gtk-cursor-theme-size=0
gtk-toolbar-style=GTK_TOOLBAR_ICONS
gtk-toolbar-icon-size=GTK_ICON_SIZE_LARGE_TOOLBAR
gtk-button-images=1
gtk-menu-images=1
gtk-enable-event-sounds=1
gtk-enable-input-feedback-sounds=1
gtk-xft-antialias=1
gtk-xft-hinting=1
gtk-xft-hintstyle=hintslight
gtk-xft-rgba=rgb
EOF

cat <<'EOF' > /home/aether/.gtkrc-2.0
gtk-theme-name="Arc-Darker"
gtk-icon-theme-name="Papirus"
gtk-font-name="Ubuntu 11"
EOF

chown aether:aether /home/aether/.gtkrc-2.0

chmod +x /home/aether/.config/openbox/autostart
chown -R aether:aether /home/aether/.config

echo "==> Configuring Plymouth daemon to use default theme..."
mkdir -p /etc/plymouth
cat <<'EOF' > /etc/plymouth/plymouthd.conf
[Daemon]
Theme=aether-boot
ShowDelay=0
DeviceTimeout=8
EOF

echo "==> Configuring initramfs modules for early KMS graphics..."
mkdir -p /etc/initramfs-tools
cat <<'EOF' >> /etc/initramfs-tools/modules
vmwgfx
vboxvideo
drm
EOF

echo "==> Creating early KMS driver loading script for initramfs..."
mkdir -p /etc/initramfs-tools/scripts/init-top
cat <<'EOF' > /etc/initramfs-tools/scripts/init-top/vmwgfx
#!/bin/sh
PREREQ=""
prereqs()
{
     echo "$PREREQ"
}
case $1 in
prereqs)
     prereqs
     exit 0
     ;;
esac

modprobe -q vmwgfx || true
modprobe -q vboxvideo || true
EOF
chmod +x /etc/initramfs-tools/scripts/init-top/vmwgfx

echo "==> Installing Plymouth boot splash..."
# Setup the custom aether-boot plymouth splash directories and configurations
mkdir -p /usr/share/plymouth/themes/aether-boot
cp /tmp/staging/themes/plymouth/* /usr/share/plymouth/themes/aether-boot/ || true
update-alternatives --install /usr/share/plymouth/themes/default.plymouth default.plymouth /usr/share/plymouth/themes/aether-boot/aether-boot.plymouth 100
update-alternatives --set default.plymouth /usr/share/plymouth/themes/aether-boot/aether-boot.plymouth || true
echo "==> Regenerating initramfs with custom Plymouth splash theme..."
update-initramfs -u -k all || update-initramfs -c -k all || true

echo "==> Creating initial seen list for desktop sync..."
mkdir -p /home/aether/.config
find /usr/share/applications /usr/local/share/applications -name "*.desktop" > /home/aether/.config/aether-desktop-sync-seen.txt || true
chown -R aether:aether /home/aether/.config

echo "==> Cleaning up system logs and apt caches..."
apt-get clean
rm -rf /var/lib/apt/lists/*
rm -rf /tmp/staging
fi

echo "==> Chroot customization sequence successfully complete!"
