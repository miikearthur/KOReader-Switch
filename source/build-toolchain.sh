#!/usr/bin/env bash
# Build devkitA64 (binutils + gcc + newlib), devkitA64 rules, general-tools,
# switch-tools and libnx from source, following devkitPro's buildscripts,
# plus hacbrewpack (for `make TARGET=switch nsp`).
# (devkitpro.org package downloads are not reachable from this network.)
set -euo pipefail

SP="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$SP/src"
DL="$SP/dl"
BUILD="$SP/tcbuild"
export DEVKITPRO="$SP/devkitpro"
export DEVKITA64="$DEVKITPRO/devkitA64"
target=aarch64-none-elf
prefix="$DEVKITA64"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
BREW=/opt/homebrew

BINUTILS_VER=2.46.0
GCC_VER=16.1.0
NEWLIB_VER=4.6.0.20260123
BINUTILS_PKGREL=1
GCC_PKGREL=1
NEWLIB_PKGREL=4

export PATH="$BREW/opt/bison/bin:$BREW/opt/flex/bin:$BREW/opt/texinfo/bin:$BREW/opt/make/libexec/gnubin:$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH"
PATCH="$(command -v gpatch || command -v patch)"
cppflags="-mmacosx-version-min=11.0 -I$BREW/include -fbracket-depth=512"
ldflags="-mmacosx-version-min=11.0 -L$BREW/lib"

mkdir -p "$BUILD" "$DEVKITPRO"
cd "$BUILD"

extract_and_patch() { # name version pkgrel ext
    if [ ! -f "extracted-$1-$2" ]; then
        echo ">>> extracting $1-$2"
        tar -xf "$DL/$1-$2.tar.$4"
        touch "extracted-$1-$2"
    fi
    if [ ! -f "patched-$1-$2" ] && [ -f "$SRC/buildscripts/patches/$1-$2-$3.patch" ]; then
        echo ">>> patching $1-$2"
        "$PATCH" -p1 -d "$1-$2" -i "$SRC/buildscripts/patches/$1-$2-$3.patch"
        touch "patched-$1-$2"
    fi
}

extract_and_patch binutils $BINUTILS_VER $BINUTILS_PKGREL xz
extract_and_patch gcc $GCC_VER $GCC_PKGREL xz
extract_and_patch newlib $NEWLIB_VER $NEWLIB_PKGREL gz

# --- binutils -----------------------------------------------------------------
mkdir -p "$target/binutils" && pushd "$target/binutils" >/dev/null
if [ ! -f configured ]; then
    CPPFLAGS="$cppflags" LDFLAGS="$ldflags" "../../binutils-$BINUTILS_VER/configure" \
        --prefix="$prefix" --target=$target \
        --disable-nls --disable-werror --disable-shared --disable-debug \
        --enable-lto --enable-plugins --enable-poison-system-directories \
        --disable-gdb --disable-gdbserver --disable-sim --disable-gprofng
    touch configured
fi
[ -f built ] || { make -j"$JOBS"; touch built; }
[ -f installed ] || { make install; touch installed; }
popd >/dev/null

# --- gcc stage 1 --------------------------------------------------------------
mkdir -p "$target/gcc" && pushd "$target/gcc" >/dev/null
if [ ! -f configured ]; then
    CPPFLAGS="$cppflags" LDFLAGS="$ldflags" \
    CFLAGS_FOR_TARGET="-O2 -ffunction-sections -fdata-sections" \
    CXXFLAGS_FOR_TARGET="-O2 -ffunction-sections -fdata-sections" \
    LDFLAGS_FOR_TARGET="" \
    "../../gcc-$GCC_VER/configure" \
        --target=$target --prefix="$prefix" \
        --enable-languages=c,c++,lto \
        --with-gnu-as --with-gnu-ld --with-gcc \
        --enable-cxx-flags='-ffunction-sections' \
        --disable-libstdcxx-verbose \
        --enable-poison-system-directories \
        --enable-threads=posix --disable-win32-registry --disable-nls --disable-debug \
        --disable-libmudflap --disable-libssp --disable-libgomp \
        --disable-libstdcxx-pch \
        --enable-libstdcxx-time=yes \
        --enable-libstdcxx-filesystem-ts \
        --with-newlib=yes \
        --with-native-system-header-dir=/include \
        --with-sysroot="$prefix/$target" \
        --enable-lto \
        --disable-tm-clone-registry \
        --disable-__cxa_atexit \
        --with-bugurl="https://devkitpro.org" \
        --with-march=armv8 --enable-multilib --with-pkgversion="devkitA64" \
        --with-gmp=$BREW --with-mpfr=$BREW --with-mpc=$BREW --with-isl=$BREW --with-zstd=$BREW
    touch configured
fi
[ -f built-stage1 ] || { make -j"$JOBS" all-gcc; touch built-stage1; }
[ -f installed-stage1 ] || { make install-gcc; touch installed-stage1; }
popd >/dev/null

# --- newlib -------------------------------------------------------------------
mkdir -p "$target/newlib" && pushd "$target/newlib" >/dev/null
if [ ! -f configured ]; then
    ( unset CFLAGS CC CXX
      CFLAGS_FOR_TARGET="-O2 -ffunction-sections -fdata-sections" \
      "../../newlib-$NEWLIB_VER/configure" \
        --disable-newlib-supplied-syscalls \
        --enable-newlib-mb \
        --disable-newlib-wide-orient \
        --enable-newlib-register-fini \
        --target=$target \
        --prefix="$prefix" )
    touch configured
fi
[ -f built ] || { make -j"$JOBS"; touch built; }
[ -f installed ] || { make install -j1; touch installed; }
popd >/dev/null

# --- gcc stage 2 --------------------------------------------------------------
pushd "$target/gcc" >/dev/null
[ -f built-stage2 ] || { make -j"$JOBS"; touch built-stage2; }
[ -f installed-stage2 ] || { make install-strip; touch installed-stage2; }
popd >/dev/null

# GCC's <limits.h> was generated during stage 1, before newlib's headers were
# installed, so it doesn't chain to newlib's (no PATH_MAX & co): regenerate it.
if [ ! -f fixed-limits-h ]; then
    cat "gcc-$GCC_VER/gcc/limitx.h" "gcc-$GCC_VER/gcc/glimits.h" "gcc-$GCC_VER/gcc/limity.h" \
        > "$prefix/lib/gcc/$target/$GCC_VER/include/limits.h"
    touch fixed-limits-h
fi

# --- devkitA64 rules ----------------------------------------------------------
if [ ! -f installed-rules ]; then
    # The rules Makefile hardcodes /opt/devkitpro: just copy the files.
    mkdir -p "$DEVKITA64"
    cp -v "$SRC/devkita64-rules/base_rules" "$SRC/devkita64-rules/base_tools" "$DEVKITA64/"
    touch installed-rules
fi

# --- general-tools (bin2s, ...) -----------------------------------------------
if [ ! -f installed-general-tools ]; then
    rm -rf general-tools && cp -R "$SRC/general-tools" general-tools
    ( cd general-tools && ./autogen.sh && ./configure --prefix="$DEVKITPRO/tools" && make -j"$JOBS" && make install )
    touch installed-general-tools
fi

# --- switch-tools (elf2nro, nacptool, ...) ------------------------------------
if [ ! -f installed-switch-tools ]; then
    rm -rf switch-tools && cp -R "$SRC/switch-tools" switch-tools
    ( cd switch-tools && ./autogen.sh && \
      PKG_CONFIG_PATH="$BREW/opt/lz4/lib/pkgconfig:$BREW/lib/pkgconfig" \
      CPPFLAGS="-I$BREW/include" LDFLAGS="-L$BREW/lib" \
      ./configure --prefix="$DEVKITPRO/tools" && make -j"$JOBS" && make install )
    touch installed-switch-tools
fi

# --- libnx --------------------------------------------------------------------
if [ ! -f installed-libnx ]; then
    make -C "$SRC/libnx/nx" -j"$JOBS" DEVKITPRO="$DEVKITPRO"
    make -C "$SRC/libnx/nx" install DEVKITPRO="$DEVKITPRO"
    touch installed-libnx
fi

# --- hacbrewpack (NSP packaging) ------------------------------------------------
# The original repository (The-4n/hacBrewPack) is gone: use a fork of its last commit.
if [ ! -f installed-hacbrewpack ]; then
    rm -rf hacBrewPack && git clone https://github.com/dragonflylee/hacBrewPack.git
    ( cd hacBrewPack && git checkout 745b16e && cp config.mk.template config.mk && make -j"$JOBS" && \
      cp hacbrewpack "$DEVKITPRO/tools/bin/" )
    touch installed-hacbrewpack
fi

echo ">>> toolchain ready in $DEVKITPRO"
"$DEVKITA64/bin/aarch64-none-elf-gcc" --version | head -1
