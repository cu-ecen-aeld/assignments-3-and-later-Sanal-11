#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

# OUTDIR=~/tmp/aeld
OUTDIR=/home/win/workspace/coursera/tmp
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
WORK_SPACE=/home/win/workspace/coursera/assignment-1-Sanal-11

export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

# cd "$OUTDIR"
# if [ ! -d "${OUTDIR}/linux-stable" ]; then
#     #Clone only if the repository does not exist.
# 	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
# 	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
# fi
# if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
#     cd linux-stable
#     echo "Checking out version ${KERNEL_VERSION}"
#     git checkout ${KERNEL_VERSION}

#     # TODO: Add your kernel build steps here
     
#     make mrproper
#     make defconfig
    
#     make Image modules dtbs -j$(nproc)
#     cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
# fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,sbin,etc,proc,sys,urs/{bin,sbin},dev,lib,lib64,home}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

else
    cd busybox
fi

# TODO: Make and install busybox
make  ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp ${SYSROOT}/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp ${SYSROOT}/libm.so.6 ${OUTDIR}/rootfs/lib64
cp ${SYSROOT}/libc.so.6 ${OUTDIR}/rootfs/lib64
cp ${SYSROOT}/libresolv.so.2 ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
cd ..
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${WORK_SPACE}/finder-app/
echo "make writer $(pwd)"
make CROSS_COMPILE=${CROSS_COMPILE}


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${WORK_SPACE}/finder-app/finder.sh ${OUTDIR}/rootfs/home
cp ${WORK_SPACE}/finder-app/finder-test.sh ${OUTDIR}/rootfs/home
cp ${WORK_SPACE}/finder-app/autorun-qemu.sh ${OUTDIR}/rootfs/home
cp ${WORK_SPACE}/finder-app/writer ${OUTDIR}/rootfs/home
sudo cp -rL ${WORK_SPACE}/finder-app/conf ${OUTDIR}/rootfs/home


# TODO: Chown the root directory
cd ${OUTDIR}/rootfs

sudo chown root:root .

# TODO: Create initramfs.cpio.gz
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ${OUTDIR}/initramfs.cpio.gz
# find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
# gzip -f ${OUTDIR}/initramfs.cpio
echo "output location: $(pwd)"
echo "initramfs.cpio.gz successfully created and copied"
