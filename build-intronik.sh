#!/bin/sh -e
# settings start here
DIR=$PWD
echo building intronik kernel for beagle bone
export BUILD=intronik1.1
export DISTRO=cross
export DEBARCH=armhf
export ARCH=arm
export config="intronik_defconfig"
export CC=${DIR}/../dl/gcc-linaro-arm-linux-gnueabihf-4.9-2014.08_linux/bin/arm-linux-gnueabihf-
export KERNEL=${DIR}/bb-kernel/KERNEL
export DEPLOY=${DIR}/intronik-deploy
export KERNEL_REL=3.14
export KERNEL_TAG=${KERNEL_REL}-rc3
# actual script
export LOCALVERSION=-${BUILD}
export CROSS_COMPILE="${CC}"
mkdir -p ${DEPLOY}
if [ $(which nproc) ] ; then
    export CORES=$(nproc)
else
    export CORES=1
fi
if false; then
	cd ${KERNEL}
	echo nop
	make --jobs=${CORES}    ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} distclean
	make --jobs=${CORES}    ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} ${config}
	make --jobs=${CORES}    ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} menuconfig
	make --jobs=${CORES}    ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} zImage modules
	make --jobs=${CORES} -s ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} firmware_install INSTALL_FW_PATH=${DEPLOY}/fw
	make --jobs=${CORES} -s ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} modules_install INSTALL_MOD_PATH=${DEPLOY}/modules
	make --jobs=${CORES} -s ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} dtbs_install INSTALL_DTBS_PATH=${DEPLOY}/dtbs
	make --jobs=${CORES} -s ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} zinstall INSTALL_PATH=${DEPLOY}/
	fakeroot make --jobs=3 -s ARCH=${ARCH} CROSS_COMPILE="${CC}" LOCALVERSION=-${BUILD} deb-pkg KBUILD_DEBARCH=armhf DISTRO=cross KDEB_PKGVERSION=1
fi
echo done
