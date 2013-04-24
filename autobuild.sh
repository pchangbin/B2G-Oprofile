#!/bin/bash

TOP=$(dirname $(readlink -e $0))
echo $TOP

pushd $TOP 2>&1 >/dev/null

# Build host version
mkdir -p $TOP/build/host && pushd $TOP/build/host
$TOP/configure --prefix=$TOP/output/host
make install-strip -j8
popd

# install NDK toolchain
#ffos ndk-at $TOP/build/ndk

# Build popt library
mkdir -p $TOP/build/popt && pushd $TOP/build/popt
$TOP/popt-1.16/configure --prefix=$NDK/sysroot/usr
make install-strip -j8
popd

# Build binutils
mkdir -p $TOP/build/binutils && pushd $TOP/build/binutils
$TOP/binutils-2.23.2/configure --prefix=$NDK/sysroot/usr
make -j8 && make install-strip STRIPPROG=arm-linux-androideabi-strip
popd

# Make some useful links
ln -sf $PWD/build/binutils/intl/libintl.a $NDK/sysroot/usr/lib/
ln -sf $PWD/build/binutils/bfd/libbfd.a $NDK/sysroot/usr/lib/
ln -sf $PWD/build/binutils/bfd/bfd.h $NDK/sysroot/usr/include/

# Build target version
mkdir -p $PWD/build/target && pushd $PWD/build/target
$TOP/configure --host=arm-eabi \
	--prefix=/system/vendor \
	--with-kernel=$ANDROID_PRODUCT_OUT/obj/KERNEL_OBJ/usr/ \
	--with-binutils=$TOP/binutils-2.23.2 \
	CC=arm-linux-androideabi-gcc \
	CXX=arm-linux-androideabi-g++ \
	CFLAGS=-DNDK_BUILD \
	CXXFLAGS=-DNDK_BUILD
make install-strip DESTDIR=$TOP/output/target STRIP=arm-linux-androideabi-strip -j8
popd

# Remove unnecessary files
#rm -rf output/target/system/vendor/bin/oprofiled output/target/system/vendor/bin/opcontrol output/target/system/vendor/include/ output/target/system/vendor/lib/ output/target/system/vendor/share/doc/ output/target/system/vendor/share/man/ output/target/system/vendor/share/oprofile/alpha/  output/target/system/vendor/share/oprofile/avr32/ output/target/system/vendor/share/oprofile/i386/ output/target/system/vendor/share/oprofile/ia64/ output/target/system/vendor/share/oprofile/mips/ output/target/system/vendor/share/oprofile/mpcore/ output/target/system/vendor/share/oprofile/ppc output/target/system/vendor/share/oprofile/ppc64/ output/target/system/vendor/share/oprofile/rtc/ output/target/system/vendor/share/oprofile/s390/ output/target/system/vendor/share/oprofile/tile/ output/target/system/vendor/share/oprofile/x86-64/ output/target/system/vendor/share/oprofile/xscale1/ output/target/system/vendor/share/oprofile/xscale2/ output/host/include/ output/host/lib/

popd

