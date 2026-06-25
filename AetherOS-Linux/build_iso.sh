#!/bin/bash
# ==============================================================
# AETHEROS LINUX — LIVE ISO MASTER BUILD SCRIPT
# This script orchestrates the creation of a bootable custom live Linux ISO.
# ==============================================================

set -e

# Path declarations
PROJECT_DIR="/mnt/c/Users/defaultuser0/Desktop/Antigravity"
BUILD_DIR="/tmp/aetheros-iso"
CHROOT_DIR="$BUILD_DIR/chroot"
IMAGE_DIR="$BUILD_DIR/image"
OUTPUT_ISO="$PROJECT_DIR/AetherOS-64/AetherOS.iso"

# Safety Cleanup Trap
cleanup() {
    echo "==> Cleaning up mountpoints to prevent workspace locking..."
    umount -lf $CHROOT_DIR/dev/pts 2>/dev/null || true
    umount -lf $CHROOT_DIR/dev 2>/dev/null || true
    umount -lf $CHROOT_DIR/sys 2>/dev/null || true
    umount -lf $CHROOT_DIR/proc 2>/dev/null || true
}
trap cleanup EXIT

echo "==> Cleaning up previous build directories..."
cleanup
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR $IMAGE_DIR

CACHE_DIR="/var/cache/aetheros"
CACHE_FILE="$CACHE_DIR/chroot-base.tar.gz"

if [ -f "$CACHE_FILE" ]; then
    echo "==> Restoring base chroot filesystem from cache..."
    tar -xzf "$CACHE_FILE" -C $BUILD_DIR
else
    echo "==> Base cache not found. Performing full bootstrap and package installation..."
    mkdir -p $CHROOT_DIR
    
    echo "==> Bootstrapping minimal Ubuntu Jammy (22.04 LTS) rootfs..."
    debootstrap --arch=amd64 jammy $CHROOT_DIR http://archive.ubuntu.com/ubuntu/
    
    # Set up chroot base install script
    cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/chroot_setup.sh $CHROOT_DIR/tmp/chroot_setup.sh
    chmod +x $CHROOT_DIR/tmp/chroot_setup.sh
    
    # Mounting virtual filesystems for base installation...
    mount -t proc proc $CHROOT_DIR/proc
    mount -t sysfs sys $CHROOT_DIR/sys
    mount --bind /dev $CHROOT_DIR/dev
    mount --bind /dev/pts $CHROOT_DIR/dev/pts
    
    echo "==> Entering chroot for base package installation..."
    chroot $CHROOT_DIR /bin/bash /tmp/chroot_setup.sh --base
    
    # Unmounting virtual filesystems...
    umount $CHROOT_DIR/proc || true
    umount $CHROOT_DIR/sys || true
    umount $CHROOT_DIR/dev/pts || true
    umount $CHROOT_DIR/dev || true
    
    rm -f $CHROOT_DIR/tmp/chroot_setup.sh
    
    echo "==> Creating base cache tarball..."
    mkdir -p $CACHE_DIR
    tar -czf "$CACHE_FILE" -C $BUILD_DIR chroot
fi

# Now apply fresh customizations on top of the base chroot
echo "==> Preparing staging directories inside chroot..."
STAGING_DIR="$CHROOT_DIR/tmp/staging"
rm -rf $STAGING_DIR
mkdir -p $STAGING_DIR/themes/plymouth

echo "==> Copying Copilot app and assets..."
mkdir -p $STAGING_DIR/copilot
cp -r $PROJECT_DIR/AetherOS-64/AetherOS-Linux/configs/copilot/* $STAGING_DIR/copilot/

echo "==> Copying local Google Chrome deb package..."
cp $PROJECT_DIR/AetherOS-64/google-chrome-stable_current_amd64.deb $STAGING_DIR/google-chrome.deb

echo "==> Copying Plymouth boot themes..."
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/themes/plymouth/* $STAGING_DIR/themes/plymouth/

echo "==> Copying custom wallpapers..."
mkdir -p $CHROOT_DIR/usr/share/backgrounds
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/themes/*.png $CHROOT_DIR/usr/share/backgrounds/

echo "==> Copying application desktop configuration files..."
mkdir -p $STAGING_DIR/copilot/launchers
cp "$PROJECT_DIR/AetherOS-64/AetherOS-Linux/configs/"*.desktop "$CHROOT_DIR/tmp/staging/"
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/configs/chrome_setup.py $STAGING_DIR/copilot/launchers/chrome_setup.py
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/configs/chrome_setup.sh $STAGING_DIR/copilot/launchers/chrome_setup.sh

echo "==> Setting up chroot script hooks..."
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/chroot_setup.sh $CHROOT_DIR/tmp/chroot_setup.sh
chmod +x $CHROOT_DIR/tmp/chroot_setup.sh
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/configs/aether-install $CHROOT_DIR/usr/local/bin/aether-install
cp $PROJECT_DIR/AetherOS-64/AetherOS-Linux/configs/aether-installer-gui.py $CHROOT_DIR/usr/local/bin/aether-installer-gui.py

# Mounting virtual filesystems for customization...
mount -t proc proc $CHROOT_DIR/proc
mount -t sysfs sys $CHROOT_DIR/sys
mount --bind /dev $CHROOT_DIR/dev
mount --bind /dev/pts $CHROOT_DIR/dev/pts

echo "==> Entering chroot environment for custom configuration..."
chroot $CHROOT_DIR /bin/bash /tmp/chroot_setup.sh --config

echo "==> Unmounting virtual filesystems..."
umount $CHROOT_DIR/proc || true
umount $CHROOT_DIR/sys || true
umount $CHROOT_DIR/dev/pts || true
umount $CHROOT_DIR/dev || true

echo "==> Generating compressed live root filesystem (SquashFS)..."
mkdir -p $IMAGE_DIR/casper
mksquashfs $CHROOT_DIR $IMAGE_DIR/casper/filesystem.squashfs -noappend

echo "==> Extracting kernel and ramdisk headers..."
KERNEL_FILE=$(find $CHROOT_DIR/boot/ -name "vmlinuz-*" | head -n 1)
INITRD_FILE=$(find $CHROOT_DIR/boot/ -name "initrd.img-*" | head -n 1)

if [ -z "$KERNEL_FILE" ] || [ -z "$INITRD_FILE" ]; then
    echo "ERROR: Kernel (vmlinuz) or Ramdisk (initrd.img) was not found in chroot /boot/!"
    exit 1
fi

cp "$KERNEL_FILE" $IMAGE_DIR/casper/vmlinuz
cp "$INITRD_FILE" $IMAGE_DIR/casper/initrd.img

echo "==> Generating custom GRUB configuration..."
mkdir -p $IMAGE_DIR/boot/grub
cat <<EOF > $IMAGE_DIR/boot/grub/grub.cfg
set default="0"
set timeout=5

# Load video modules for maximum compatibility
insmod all_video
insmod vbe
insmod vga
insmod gfxterm

set gfxmode=1920x1080,1024x768,auto
set gfxpayload=keep

menuentry "AetherOS Linux v0.1" {
    linux /casper/vmlinuz boot=casper quiet splash video=1920x1080 vt.handoff=7 ---
    initrd /casper/initrd.img
}

menuentry "AetherOS Linux v0.1 (Verbose Mode)" {
    linux /casper/vmlinuz boot=casper nomodeset video=1920x1080 vt.handoff=7 ---
    initrd /casper/initrd.img
}

menuentry "AetherOS Linux v0.1 (Failsafe Text)" {
    linux /casper/vmlinuz boot=casper nomodeset text systemd.unit=multi-user.target ---
    initrd /casper/initrd.img
}

menuentry "AetherOS Linux v0.1 (Recovery - startx)" {
    linux /casper/vmlinuz boot=casper nomodeset video=1920x1080 single systemd.unit=rescue.target ---
    initrd /casper/initrd.img
}
EOF

echo "==> Building hybrid bootable ISO..."
grub-mkrescue -o $OUTPUT_ISO $IMAGE_DIR

echo "==> Cleaning up build workspace..."
cleanup
rm -rf $BUILD_DIR

echo "==> AetherOS Linux Live ISO successfully compiled at: $OUTPUT_ISO"
