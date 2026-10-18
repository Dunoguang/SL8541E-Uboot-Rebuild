#!/bin/sh
# Build U-Boot for Unisoc SL8541E / SC9832E (sl8541e_1h10).
# Tested with aarch64-linux-gnu-gcc 13.3 / 16.1 on an aarch64 host.
set -e
cd "$(dirname "$0")"

CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
JOBS=${JOBS:-$(nproc)}
export CROSS_COMPILE
export KBUILD_OUTPUT=${KBUILD_OUTPUT:-./out}

# The vendor tree ships a prebuilt x86-64 dtc, which dies with
# "Exec format error" on an arm64 host. Replace it with the system one.
if [ -x tools/dtc/dtc ] && ! tools/dtc/dtc --version >/dev/null 2>&1; then
    echo ">> replacing vendor tools/dtc/dtc with $(command -v dtc)"
    cp "$(command -v dtc)" tools/dtc/dtc
fi

# -std=gnu11: gcc 16 defaults to gnu23, where 'bool' is a keyword and
# the vendor headers (typedef int bool) no longer compile.
export KCFLAGS="-std=gnu11 -Wno-implicit-function-declaration \
-Wno-incompatible-pointer-types -Wno-int-conversion -Wno-implicit-int \
-Wno-builtin-declaration-mismatch -Wno-return-mismatch"

# secure boot switches that BspUBoot.mk passes to the vendor build
export UBOOT_SECURE_BOOT_FLAG="-DCONFIG_SECBOOT -DCONFIG_SPRD_SECBOOT -DCONFIG_VBOOT_V2 -DPKCS1_PSS_FLAG"
export UBOOT_TOS_TRUSTY_FLAG="-DTOS_TRUSTY"
export UBOOT_TEECFG_CUSTOM_FLAG="-DCONFIG_TEECFG_CUSTOM"
export UBOOT_CHIP_UID_FLAG="-DCONFIG_CHIP_UID"
export UBOOT_KERNEL_CMDLINE_FLAG="-DCONFIG_BOARD_KERNEL_CMDLINE"
export UBOOT_CALIMODE_USE_BOOTIMG_FLAG="-DCONFIG_CALIMODE_USE_BOOTIMG"

make ARCH=arm sl8541e_1h10_defconfig
make ARCH=arm DEVICE_TREE=sl8541e_1h10 -j"$JOBS"

ls -l out/u-boot.bin out/u-boot-dtb.bin
