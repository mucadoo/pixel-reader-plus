#!/bin/bash
set -eu
# Cross-builds the reader for 64-bit ARM Linux with glibc 2.17, using Zig as the compiler.
#
#   ZIG          the zig executable (default: zig on the PATH)
#   INC          a folder holding the headers of SDL 1.2, SDL_ttf, SDL_image, libzip and libxml2
#                (for example the /usr/include of the arm64 -dev packages unpacked)
#   DEVICE_LIBS  a folder holding libSDL-1.2.so.0 and libxml2.so.2 copied from the device
#   BUILD        where the work goes (default: build/aarch64 in this tree)
#
# The result is $BUILD/reader, and $BUILD/reader.stripped without the symbols.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ZIG=${ZIG:-zig}
INC=${INC:?set INC to a folder holding the headers of SDL 1.2, SDL_ttf, SDL_image, libzip and libxml2}
BUILD=${BUILD:-$ROOT/build/aarch64}
OBJ=$BUILD/obj
LIBS=$BUILD/libs
OUT=$BUILD/reader
mkdir -p "$OBJ" "$LIBS"
cd "$ROOT"

SRC_LIBS=${DEVICE_LIBS:?set DEVICE_LIBS to a folder holding libSDL-1.2.so.0 and libxml2.so.2 copied from the device}
if [ ! -e "$SRC_LIBS/libSDL-1.2.so.0" ]; then
  echo "libSDL-1.2.so.0 not found in $SRC_LIBS"
  exit 1
fi
cp -f "$SRC_LIBS/libSDL-1.2.so.0" "$LIBS/"
cp -f "$SRC_LIBS/libxml2.so.2" "$LIBS/"
ln -sfn libSDL-1.2.so.0 "$LIBS/libSDL-1.2.so"
ln -sfn libxml2.so.2 "$LIBS/libxml2.so"

# Empty SONAME stubs are not enough: Zig's lld rejects --unresolved-symbols.
# Export the symbols this tree actually references. Versions stay unset so the
# device loader binds them to the firmware copies.
cat > "$LIBS/stub_ttf.c" << 'EOF'
void TTF_CloseFont() {}
void TTF_Init() {}
void TTF_OpenFont() {}
void TTF_RenderUTF8_Blended() {}
void TTF_RenderUTF8_Shaded() {}
void TTF_SetFontStyle() {}
void TTF_SizeUTF8() {}
void TTF_FontAscent() {}
void TTF_FontHeight() {}
void TTF_GlyphMetrics() {}
EOF
cat > "$LIBS/stub_img.c" << 'EOF'
void IMG_LoadTyped_RW() {}
EOF
cat > "$LIBS/stub_zip.c" << 'EOF'
void zip_close() {}
void zip_fclose() {}
void zip_fopen() {}
void zip_fread() {}
void zip_open() {}
void zip_stat() {}
void zip_source_buffer() {}
void zip_file_add() {}
void zip_source_free() {}
void zip_discard() {}
void zip_get_num_entries() {}
void zip_get_name() {}
void zip_stat_index() {}
void zip_fopen_index() {}
EOF
"$ZIG" cc -target aarch64-linux-gnu.2.17 -shared -Wl,-soname,libSDL_ttf-2.0.so.0 "$LIBS/stub_ttf.c" -o "$LIBS/libSDL_ttf-2.0.so"
"$ZIG" cc -target aarch64-linux-gnu.2.17 -shared -Wl,-soname,libSDL_image-1.2.so.0 "$LIBS/stub_img.c" -o "$LIBS/libSDL_image-1.2.so"
"$ZIG" cc -target aarch64-linux-gnu.2.17 -shared -Wl,-soname,libzip.so.5 "$LIBS/stub_zip.c" -o "$LIBS/libzip.so"

fail=0
files=$(find src -name '*.cpp' -o -name 'SDL_rotozoom.c' | grep -v '/tests/' | grep -v '/sandbox/')
objs=()
for f in $files; do
  obj=$OBJ/$(echo "$f" | tr '/' '_').o
  if [[ "$f" == *.c ]]; then
    compiler=cc
    std=
  else
    compiler=c++
    std="-std=c++17"
  fi
  echo "CC $f"
  if ! "$ZIG" "$compiler" -target aarch64-linux-gnu.2.17 $std -O2 -Isrc -I"$INC" -I"$INC/libxml2" -c "$f" -o "$obj"; then
    fail=1
  fi
  objs+=("$obj")
done
if [ "$fail" != 0 ]; then
  exit 1
fi

echo LINK
"$ZIG" c++ -target aarch64-linux-gnu.2.17 -O2 -o "$OUT" "${objs[@]}" \
  -L"$LIBS" \
  -Wl,--no-as-needed \
  -lSDL-1.2 -lSDL_ttf-2.0 -lSDL_image-1.2 -lzip -lxml2 \
  -Wl,--as-needed
echo DONE
"$ZIG" objcopy --strip-all "$OUT" "$OUT.stripped"
readelf -d "$OUT"
echo '--- versions ---'
readelf -V "$OUT"
file "$OUT"
