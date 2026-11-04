#!/usr/bin/env bash
set -euo pipefail
root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cache="$root/.tools"
mkdir -p "$cache"
for tool in curl sha256sum tar perl make cmake ninja x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++; do
    command -v "$tool" >/dev/null || { printf 'Missing tool: %s\n' "$tool" >&2; exit 1; }
done
fetch() {
    local name="$1" url="$2" digest="$3"
    if [[ ! -f "$cache/$name" ]]; then
        curl --fail --location --retry 2 "$url" -o "$cache/$name.part"
        mv -- "$cache/$name.part" "$cache/$name"
    fi
    printf '%s  %s\n' "$digest" "$cache/$name" | sha256sum --check --status
}
fetch SDL2-devel-2.32.10-mingw.tar.gz \
    https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz \
    83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
fetch openssl-3.6.5.tar.gz \
    https://github.com/openssl/openssl/releases/download/openssl-3.6.5/openssl-3.6.5.tar.gz \
    a2157c2830efdec3788939b00c9b0638306d3f0bbb76dc4832ee503bb397df98
fetch freetype-2.14.3.tar.xz \
    https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz \
    36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f
if [[ ! -f "$cache/SDL2-2.32.10/x86_64-w64-mingw32/include/SDL2/SDL.h" ]]; then
    tar -xzf "$cache/SDL2-devel-2.32.10-mingw.tar.gz" -C "$cache"
fi
if [[ ! -f "$cache/openssl-win64/lib/libcrypto.a" || ! -f "$cache/openssl-win64/include/openssl/bn.h" ]]; then
    if [[ ! -f "$cache/openssl-3.6.5/Configure" ]]; then
        tar -xzf "$cache/openssl-3.6.5.tar.gz" -C "$cache"
    fi
    (
        cd "$cache/openssl-3.6.5"
        ./Configure mingw64 no-shared no-tests no-apps no-docs no-module no-dso \
            --cross-compile-prefix=x86_64-w64-mingw32- \
            --prefix="$cache/openssl-win64" --libdir=lib
        make -j4
        make install_sw
    )
fi
if [[ ! -f "$cache/freetype-win64/lib/libfreetype.a" ]]; then
    if [[ ! -f "$cache/freetype-2.14.3/CMakeLists.txt" ]]; then
        tar -xJf "$cache/freetype-2.14.3.tar.xz" -C "$cache"
    fi
    cmake -S "$cache/freetype-2.14.3" -B "$cache/freetype-build-win" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw64.cmake" \
        -DCMAKE_INSTALL_PREFIX="$cache/freetype-win64" -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
    cmake --build "$cache/freetype-build-win" --parallel 4
    cmake --install "$cache/freetype-build-win"
fi
printf 'Windows x86-64 dependencies ready in %s\n' "$cache"
