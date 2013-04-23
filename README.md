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
