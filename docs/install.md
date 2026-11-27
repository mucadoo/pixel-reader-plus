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
