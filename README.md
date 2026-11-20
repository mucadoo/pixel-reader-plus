# Spruce E-Reader

An ebook reader for the Miyoo Flip on spruceOS. It opens epub and txt books, follows the book's own type sizes, shows pictures, and can look a word up in an offline English dictionary or highlight it.

On spruce the app is **E-Reader**. This build replaces the stock Pixel Reader binary. Your books, reading position, and highlights stay on the card.

## Put a book on the card

Copy `.epub` or `.txt` files to `Roms/EBOOKS` on the SD card. Open **E-Reader**, pick the folder if it is not already there, and press **A** on the book.

The reader remembers the folder and the position in the book. The next time you open that book, it returns to the same place.

## Reading

These are the printed buttons on the Flip.

| Button | While reading |
| --- | --- |
| D-pad up / down | Previous or next line. Hold to keep moving. |
| D-pad left / right | Previous or next page. |
| L1 / R1 | Previous or next page. Settings can move this to L2 / R2. |
| A | Show or hide the progress line. |
| B | Close the book and return to the file list. |
| X | Settings. |
| Y | Select a word. |
| Select | Chapter list. Press Select again to close it. |
| Menu | Quit the app. A tap of Menu quits. A Menu chord, such as the brightness shortcut, does not. |

The progress line shows the book title and how far through it you are. Settings can switch that between the whole book and the current chapter.

## Word lookup and highlights

Press **Y** on a page. A cursor sits on a word.

| Button | While a word is selected |
| --- | --- |
| D-pad | Move to another word. Hold to repeat. Left at the start of the page, or right at the end, turns the page. |
| A | Open **Look up** and **Highlight**. If the word is already highlighted, the second row is **Remove highlight**. |
| B or Y | Leave word selection. The shoulders still turn the page. |

**Look up** opens the definition full screen. Up and down scroll a long definition. Any other button closes it.

The dictionary is English only. It is WordNet 3.1, one gloss per word, the first sense that was kept. Lookup tries the word, then the word without `'s`, then the word without a trailing `s`. It does not know phrases, names, or other languages. If the dictionary file is missing, Look up says there is no entry.

**Highlight** marks that word and saves it with the book. A highlight is a bar behind the word. It is still there after you close the book. Removing it deletes that one mark. Installing the reader again does not delete highlights.

## How the text is shown

The reader reads the book's stylesheet, not only heading tags. A title written as a normal paragraph with a large size is drawn large. A chapter number and a chapter title keep the sizes the book asked for, and centered text stays centered.

- Bold and italic in the book are kept, including italics that the book marks with a class instead of an `<i>` tag.
- A quoted block is indented and italic.
- Preformatted text uses a monospace font.
- A very large first letter, the kind a book uses as a drop cap, stays large on that line. The lines after it go back to body size.
- Real headings (`h1` to `h6`) are still larger and bold when the book does not give them a size. `h1` and `h2` are centered unless the stylesheet says otherwise.

The size you pick in settings is the body size. Titles are scaled from that, and they are kept between 12 and 72 pixels so a title cannot become unreadably small or larger than the screen.

## Pictures

Jpeg, png, and gif pictures in an epub are drawn in the page. A wide picture is scaled down to the screen width. A tall picture continues on the following lines, so you scroll through it.

The cover, a title-page portrait, and photo inserts are ordinary pictures in the book. If a picture cannot be read, the page shows the picture's path in brackets instead of a blank gap.

## Chapters

**Select** opens the chapter list from the book's table of contents. Up and down move, **A** jumps to the chapter, **B** or **Select** closes the list. A book with no table of contents shows an empty list. There is no separate next-chapter button.

## Settings

Press **X**. Up and down move between rows. Left and right change the selected row. **B** closes settings. The choices are saved.

- **Theme.** `spruce` is the dark theme with cream text. The others are night contrast, light contrast, sepia, and vampire.
- **Font size.** Body text, from 18 to 40. Titles scale with it.
- **Font.** The fonts shipped in `resources/fonts`, including DejaVu Sans, DejaVu Serif, and Merriweather. Merriweather uses its bold file for bold text. The other fonts use a synthetic bold.
- **Shoulder keymap.** `LR` uses L1 and R1 to turn pages. `L2R2` uses L2 and R2 instead.
- **Progress.** Percentage of the whole book, or percentage of the current chapter.

**A** while reading shows or hides the progress line. That choice is saved too.

## What is saved

The reader writes under `App/PixelReader/.pixel_reader_store`.

- Reading position and the last folder you browsed.
- Theme, font, font size, shoulder buttons, and whether the progress line is shown.
- Highlights, one file per book, named from the book file.

Books themselves are not copied or modified. A later install of this reader replaces the program and the dictionary, and leaves the store alone.

## Install on spruce

The Flip build is installed by [spruce-patch](https://github.com/mucadoo/spruce-patch). On Windows, with the spruce card plugged in, run that installer's `install.bat` and choose **E-Reader**. It copies:

- `App/PixelReader/reader`
- `App/PixelReader/resources/dictionary/english.txt`
- the WordNet notice next to that dictionary

Quit E-Reader if it is open, eject the card, and open E-Reader again. The new reader is used the next time the app starts. A spruce reinstall puts the stock reader back. Run the installer again and choose E-Reader to restore this one.

The stock reader that was on the card the first time is kept in the installer's backup folder (`spruce-patch-backup-<date>` in your user profile).

## Building

The Flip binary is aarch64, dynamically linked, and needs glibc 2.17. It uses the device's SDL 1.2, SDL_ttf, SDL_image, libzip, and libxml2. Jpeg, png, and gif pictures are decoded inside the reader, so a firmware SDL_image without jpeg still shows them. C++ is linked statically.

The source layout matches [Pixel Reader](https://github.com/ealang/pixel-reader). `make` builds the desktop and test targets. The Flip binary is cross-built from this tree for `aarch64-linux-gnu` with glibc 2.17.

## Credits and licenses

The program is GPL. See `LICENSE`. It is based on [Pixel Reader](https://github.com/ealang/pixel-reader) by ealang. The spruce color theme comes from the [Sundownersport fork](https://github.com/Sundownersport/pixel-reader).

The English dictionary in `resources/dictionary/english.txt` is generated from WordNet 3.1. Princeton's notice is in `resources/dictionary/LICENSE.txt`.

Picture decoding uses [stb_image](https://github.com/nothings/stb), public domain, in `src/extern/stb/stb_image.h`.
