# Spruce E-Reader

An ebook reader for the Miyoo Flip on spruceOS. It opens epub and txt books, follows the book's own type sizes, shows pictures, and can look a word up in an offline English dictionary or highlight it.

On spruce the app is **E-Reader**. This build replaces the stock Pixel Reader binary. Your books, reading position, and highlights stay on the card.

## Put a book on the card

Copy `.epub` or `.txt` files to `Roms/EBOOKS` on the SD card. Open **E-Reader**, pick the folder if it is not already there, and press **A** on the book.

The reader remembers the folder and the position in the book. The next time you open that book, it returns to the same place.

## The book list

The list opens on the folder you used last, or on `Roms/EBOOKS` the first time. It stays on the SD card: **Up one folder** stops at the card's top folder, and a saved folder that is not on the card is ignored. The top bar names the folder and shows your place in it, such as `3 / 12`. Each book is shown by its title, without the file extension, with its type and size on the right, such as `EPUB, 1.2 MB`. Folders sit above the books and say `Folder`. **Up one folder** is the first row inside a subfolder. If a folder has no books or folders, the list says so and tells you where to put books.

A name that is too long for its row slides once the row is selected: it waits a moment, scrolls to the end, pauses, and starts over. In the grid it slides under the selected cover.

**Select** switches between the list and the cover grid. The choice is saved. The grid shows one to four rows of covers, depending on the screen height, with the title under each cover. A book's cover is taken from the book itself (EPUB only) and shrunk in the background, so the grid never waits on it. The small pictures are kept in a `covers` folder inside the reader's store, so they appear at once the next time. A book with no cover, or a `.txt` file, gets a plain colored cover with its title and type. Folders and **Up one folder** get their own tiles.

**Y** shows the properties of the selected entry: name, type, size, and the folder it is in. Any button closes it.

| Button | In the book list and the grid |
| --- | --- |
| D-pad up / down | List: move one row. Grid: move one row of covers. Hold to keep moving. |
| D-pad left / right | List: move half a screen. Grid: move one cover. |
| L1 / R1 | Move half a screen in the list, or a full screen of covers in the grid. Settings can move this to L2 / R2. |
| A | Open the book or folder. |
| Y | Properties of the selected entry. |
| Select | Switch between the list and the grid. |
| X | Settings. |
| B | Go up one folder. In the top folder it does nothing. |
| Menu | Quit the app. |

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
| Y | Select a word near the middle of the page. Commas, periods, and quotes are left outside the selection. |
| Select | Chapter list, opened on the chapter you are reading. Press Select again to close it. |
| Start | Open the controls guide. Up and down scroll it. Any other button closes it. |
| Menu | Quit the app. A tap of Menu quits. A Menu chord, such as the brightness shortcut, does not. |

The progress line shows the chapter name and how far you are.

**Scroll** is the normal way to read. Up and down move a line, and left and right move a screen. The progress line shows the percentage, because a page number does not belong to a continuous scroll. The percentage follows the text and does not jump when you turn a screen.

**Page** counts real screens from the start of the book or chapter. The progress line shows `12 / 248` plus the percent. When you switch to Page, the reader finishes that count and then jumps to the top of the page you were on, so you are never left in the middle of a page. After that, the D-pad and the shoulder buttons turn a whole page. Up and down do not scroll a line. Each turn adds 1. A new chapter, a title page, and a picture that does not fit the rest of the page start at the top of the next page. The blank line between two paragraphs never opens a page, so a page always starts on text. The count runs in the background, so the app keeps answering buttons while a long book is counted. Until it finishes, the percentage stays on the line and those buttons move one screen. When it finishes, the reader lines up with the top of the page you are on. Changing the font counts again and lines you up the same way.

**Book** or **Chapter** applies to both. Book is the whole file. Chapter is the current chapter, so the percentage and the page count both start over at each chapter. In Chapter mode a page turn stops at the edge of the chapter. Turning past the last page of a chapter moves to the first line after it, and turning back from the first page goes to the last page of the previous chapter, so you are never stuck inside one chapter. The new chapter is counted before the page is drawn, so the screen shows the finished page and does not jump.

## Word lookup and highlights

Press **Y** on a page. A cursor sits on a word near the middle of the page, and a bar along the bottom names the buttons. A comma, period, quote, or similar mark attached to that word is not part of the selection.

| Button | While words are selected |
| --- | --- |
| D-pad | Move the cursor. Hold to repeat. Left at the start of the page, or right at the end, turns the page. Until you extend, each move is still one word. |
| Start | Keep the first word and add every word you move through. Press Start again to stop extending. The next move returns to one word. |
| A | Open a small menu over the page, with **Look up** and **Highlight**. Look up uses the word at the cursor. Highlight covers the whole selection. If that stretch is already highlighted, the second row is **Remove highlight**. **B** returns to the word. |
| B or Y | Leave word selection. The shoulders still turn the page. |

**Look up** opens the definition full screen. Up and down scroll a long definition. Any other button closes it.

The dictionary is English only. It is WordNet 3.1, one gloss per word, the first sense that was kept. Lookup tries the word, then the word without `'s`, then the word without a trailing `s`. It does not know phrases, names, or other languages. If the dictionary file is missing, Look up says there is no entry.

**Highlight** marks the selected words and saves them with the book. A highlight is a bar behind each word. Punctuation between the words is not painted. It is still there after you close the book. Removing it deletes the marks that overlap the selection. Installing the reader again does not delete highlights.

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

**Select** opens the chapter list from the book's table of contents, on the chapter you are reading. A mark stays on that chapter while you move. The corner shows which row is selected. Up and down move, **A** jumps to the chapter, **B** or **Select** closes the list. A book with no table of contents does not open the list. There is no separate next-chapter button.

## Settings

Press **X**. The book stays behind the menu. Each row has the name on the left and the choice on the right. Up and down move. Left and right change the choice. **B** closes settings. The choices are saved.

- **Theme.** The menu shows Spruce, Night, Light, Sepia, and Vampire. Spruce is the dark theme with cream text.
- **Font size.** Body text, from 18 to 40. Titles scale with it.
- **Font.** The fonts shipped in `resources/fonts`, including DejaVu Sans, DejaVu Serif, and Merriweather. Merriweather uses its bold file for bold text. The other fonts use a synthetic bold.
- **Shoulders.** `L1 / R1` uses L1 and R1 to turn pages. `L2 / R2` uses L2 and R2 instead.
- **Progress.** `Book` or `Chapter`. The percentage, and the page count in Page mode, both follow that choice.
- **Reading.** `Scroll` shows the percentage. `Page` shows a real page count that moves one number at a time.
- **Controls.** Press **A** to open the same shortcut guide that **Start** opens while reading.

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
