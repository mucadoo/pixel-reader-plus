# Building and other devices

The Flip binary is aarch64, dynamically linked, and needs glibc 2.17. It uses the device's SDL 1.2, SDL_ttf, SDL_image, libzip, and libxml2. Jpeg, png, and gif pictures are decoded inside the reader, so a firmware SDL_image without jpeg still shows them. C++ is linked statically.

The source layout matches [Pixel Reader](https://github.com/ealang/pixel-reader). `make` builds the desktop and test targets. The 64-bit ARM binary is cross-built with `cross-compile/aarch64/build.sh`, which uses [Zig](https://ziglang.org) as the compiler and needs two things from you: `INC`, a folder with the headers of SDL 1.2, SDL_ttf, SDL_image, libzip and libxml2, and `DEVICE_LIBS`, a folder with `libSDL-1.2.so.0` and `libxml2.so.2` copied from the device, so the program links to what the firmware has. The script writes `build/aarch64/reader` and a stripped copy next to it.

## Other devices

Only the Miyoo Flip on spruceOS has been tried. Nothing below has been run on any other device. The reader should work on another handheld that has:

- a 64-bit ARM Linux with glibc 2.17 or newer;
- SDL 1.2 (a compatibility layer over SDL 2 that provides `libSDL-1.2.so.0` also works in principle), SDL_ttf, SDL_image, libzip and libxml2;
- the buttons arriving at SDL as the keys of the Miyoo firmwares (Onion and spruce): A is Space, B is left Ctrl, X is left Shift, Y is left Alt, L1 and R1 are E and T, L2 and R2 are Tab and Backspace, Select is right Ctrl, Start is Return and Menu is Escape (see `src/sys/keymap.h`). A device with other keys would need that file changed.

To set one up, put the program with its `resources` folder (fonts, dictionaries, hyphenation) in one folder and start it from anywhere; if it is started from another folder it changes to the folder of the program to find them. Settings and the reading store are kept in that folder too. These variables adjust it for a device that lays its storage out differently:

- `READER_CARD` is the storage root. The book list does not go above it. Without it the reader looks for `/mnt/SDCARD` and `/mnt/sdcard`.
- `READER_BOOKS` is the folder the book list starts in when there is no position to resume. Without it the reader uses `Roms/EBOOKS` on the storage root, or the root itself.
- `SCREEN_WIDTH` and `SCREEN_HEIGHT` set the size of the picture, which is 640x480 by default.

The tree also still holds the Miyoo Mini build files of the original Pixel Reader in `cross-compile/miyoo-mini`. They have not been used for this reader.
