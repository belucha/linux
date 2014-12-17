#!/bin/sh -e
# settings start here
DIR=$PWD
echo building intronik kernel for beagle bone
export CC=/opt/gcc-linaro-arm-linux-gnueabihf-4.9/bin/arm-linux-gnueabihf-
export CORES=3
export BUILD=intronik2
export DISTRO=cross
export DEBARCH=armhf
export ARCH=arm
export config="intronik_defconfig"
# export DEPLOY=${DIR}/../intronik-deploy
export LOCALVERSION=-${BUILD}
export CROSS_COMPILE="${CC}"
# mkdir -p ${DEPLOY}
# make --jobs=${CORES} menuconfig
# make --jobs=${CORES} zImage modules
fakeroot make -j${CORES} ARCH=arm KBUILD_DEBARCH=${DEBARCH} LOCALVERSION=-${BUILD} CROSS_COMPILE=${CC} KDEB_PKGVERSION=1${DISTRO} deb-pkg

