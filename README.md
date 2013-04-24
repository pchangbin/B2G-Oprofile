# What is
 This is clone of [Oprofile](http://oprofile.sourceforge.net).
# What for
 This will be ported for [B2G](https://wiki.mozilla.org/B2G).

# Source Repository
 Official repository of [Oprofile](http://oprofile.sourceforge.net) is
git://oprofile.git.sourceforge.net/gitroot/oprofile/oprofile

# Build
## Host version
 ```shell
$ ./autogen.sh
$ mkdir -p $PWD/build/host && pushd $PWD/build/host
$ ../../configure --prefix=$PWD/../../output/host
$ make install-strip -j8
$ popd
```
## Target version
 * Assume that NDK standalone toolchain is installed at $src_root/build/ndk
 * Target version needs popt library and binutils other than NDK included.
### popt library
 * Modified to build with NDK standalone toolchain
 * More detailed information for popt and binutils are found [here](https://docs.google.com/document/d/15RTM2khWisEeecW50zZpyNLPDnQQA7ODl7BkPYFuvh8/pub#h.fegkfqzhz005)

### binutils
 *  Modified to build with NDK standalone toolchain
 * More detailed information for popt and binutils are found [here](https://docs.google.com/document/d/15RTM2khWisEeecW50zZpyNLPDnQQA7ODl7BkPYFuvh8/pub#h.fegkfqzhz005)

### Oprofile for target
```shell
$ mkdir -p $PWD/build/target && pushd $PWD/build/target
$ ./configure --host=arm-eabi \
    --prefix=/system/vendor \
    --with-kernel=$ANDROID_PRODUCT_OUT/obj/KERNEL_OBJ/usr/ \
    --with-binutils=$PWD/../../binutils-2.23.2 \
    CC=arm-linux-androideabi-gcc \
    CXX=arm-linux-androideabi-g++ \
    CFLAGS=-DNDK_BUILD \
    CXXFLAGS=-DNDK_BUILD
$ make install-strip DESTDIR=$PWD/../../output/target STRIP=arm-linux-androideabi-strip -j8
$ popd
```

