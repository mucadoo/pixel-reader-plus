# Install

## On any device

1. Build the program, or take a built `reader` (see [Building and other devices](building.md)).
2. Put these in one folder on the device, for example `App/PixelReader`:
   - `reader`
   - `resources/fonts`
   - `resources/dictionary`: `english.txt`, `french.txt`, `portuguese.txt`, `spanish.txt`, `italian.txt`, `german.txt` and `russian.txt`, with the WordNet and Wiktionary notices
   - `resources/hyphenation`: `english.txt`, `french.txt` and `portuguese.txt`, with their notice
3. Start `reader` from the launcher of the device. It looks for `resources` in the folder it was started from, and in the folder of the program when it is not there. Settings and the reading store are kept in that folder as well.
4. Copy books to the card and open them from the list (see [Put a book on the card](../README.md#put-a-book-on-the-card)).

The reader only needs the libraries the device already has: SDL 1.2, SDL_ttf, SDL_image, libzip and libxml2.

## On spruceOS

The Miyoo Flip build is installed by [spruce-patch](https://github.com/mucadoo/spruce-patch). On Windows, with the spruce card plugged in, run that installer's `install.bat` and choose **E-Reader**. It copies the files above into `App/PixelReader` on the card.

Quit E-Reader if it is open, eject the card, and open E-Reader again. The new reader is used the next time the app starts. A spruce reinstall puts the stock reader back. Run the installer again and choose E-Reader to restore this one.

The stock reader that was on the card the first time is kept in the installer's backup folder (`spruce-patch-backup-<date>` in your user profile).

## On other systems

Nothing here has been run on a device other than the Flip with spruceOS. The steps follow the usual layout of each system as I understand it, so check the folder names against your system's own documentation. What every system needs is the same: a folder holding `reader` and `resources`, and something the launcher can start.

First check the processor. The program built by `cross-compile/aarch64/build.sh` is 64-bit ARM. Many small handhelds are 32-bit ARM (the Miyoo Mini family and the Miyoo A30, for example) and cannot run it. For the Miyoo Mini the tree still holds the build of the original Pixel Reader in `cross-compile/miyoo-mini`; it has not been used for this reader.

Then check SDL. The reader uses SDL 1.2. A system that ships only SDL 2 needs a compatibility library that provides `libSDL-1.2.so.0` (sdl12-compat) in the folder of the program, and `libSDL_ttf-2.0.so.0`, `libSDL_image-1.2.so.0`, `libzip` and `libxml2` have to be there too. Put any library of your own in a `lib` folder next to `reader` and the launcher below finds it.

A launcher script for any system, saved as `launch.sh` in the folder of the program:

```sh
#!/bin/sh
cd "$(dirname "$0")"
export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
# Only when the storage is not under /mnt/SDCARD or /mnt/sdcard, or the books are elsewhere:
# export READER_CARD=/path/to/storage
# export READER_BOOKS=/path/to/books
# Only when the screen is not 640x480:
# export SCREEN_WIDTH=640
# export SCREEN_HEIGHT=480
./reader 2>log.txt
```

Make `reader` and `launch.sh` executable (`chmod +x`). If the reader does not start, `log.txt` next to it holds what it printed.

Where the folder goes, and how the system is told about it:

- **Onion (Miyoo Mini):** a folder `App/<name>` on the card, holding `config.json` (with a `label`, `icon` and `"launch":"launch.sh"`) and `launch.sh`. The Mini needs the 32-bit build above.
- **MiniUI and NextUI:** a folder `<name>.pak` under `Tools/<platform>` on the card, with a `launch.sh` in it.
- **muOS:** a folder for the program under the application folder of muOS (`MUOS/application`), with the launcher script that muOS asks for.
- **Knulli and ROCKNIX:** a script in the `ports` folder of the ROMs (`/userdata/roms/ports` on Knulli, `/storage/roms/ports` on ROCKNIX) that starts the program from its own folder, the way `launch.sh` does.
- **Anything else with a shell:** run `./launch.sh` from a terminal or ssh first. If the reader starts there, it will start from whatever launcher you add.

The buttons arrive at SDL as the keys of the Miyoo firmwares (see `src/sys/keymap.h`). A system that sends other keys needs that file changed and the reader built again.