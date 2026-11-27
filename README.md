# Pixel Reader Plus

An ebook reader for handhelds with a small screen and a D-pad, built on [Pixel Reader](https://github.com/ealang/pixel-reader). It is tested on the Miyoo Flip with spruceOS, and it is a plain 64-bit ARM Linux program that may run on other handhelds (see [Building and other devices](docs/building.md), none tried). It opens epub, Markdown and text books, follows the book's own type sizes, shows pictures, and can look a word up in an offline English, French, or Portuguese dictionary, highlight it, bookmark places, search the book, keep a word list and reading stats, hyphenate, remember favorites and finished books, and show footnotes.

On spruceOS the app is **E-Reader**. This build replaces the stock Pixel Reader binary. Your books, reading position, and highlights stay on the card.

## Put a book on the card

Copy `.epub`, `.md` or `.txt` files to `Roms/EBOOKS` on the SD card. Open **E-Reader**, pick the folder if it is not already there, and press **A** on the book.

The reader remembers the folder and the position in the book. The next time you open that book, it returns to the same place.

## Other book formats

Besides epub, the reader opens Markdown and plain text. Other types (HTML, FB2, comics and so on) are not opened.

- **Markdown** (`.md`, `.markdown`): headings, paragraphs, bold and italic, lists, quotes, code blocks and rules. A link shows its text. Headings become the chapters and the first heading is the title.
- **Text** (`.txt`): every line is a paragraph, as before. Text in UTF-8, UTF-16 (with a byte order mark) and Latin-1 or Windows-1252 is read correctly: a file that is not valid UTF-8 is taken as Latin-1.

These books work with everything else: bookmarks, highlights, the dictionary, search, per-book layout, favorites and the book list.

## Guide

- [The book list](docs/library.md): browsing, sorting, collections, searching the library, managing books, Library tools.
- [Reading, lookups and highlights](docs/reading.md): turning pages, selecting a word, the dictionaries, highlights, how the text is laid out, pictures.
- [The book menu](docs/book-menu.md): everything the Select button opens while you read.
- [Settings](docs/settings.md): every setting, row by row.
- [What is saved](docs/saved-data.md): where your places, marks and settings live on the card.
- [Building and other devices](docs/building.md): building the program and trying it on a device other than the Flip.

## Install on spruce

The Flip build is installed by [spruce-patch](https://github.com/mucadoo/spruce-patch). On Windows, with the spruce card plugged in, run that installer's `install.bat` and choose **E-Reader**. It copies:

- `App/PixelReader/reader`
- `App/PixelReader/resources/dictionary/english.txt`, `french.txt`, and `portuguese.txt`
- `App/PixelReader/resources/hyphenation/english.txt`, `french.txt`, and `portuguese.txt`, with their notice
- the WordNet notice and the Wiktionary notice next to those dictionaries

Quit E-Reader if it is open, eject the card, and open E-Reader again. The new reader is used the next time the app starts. A spruce reinstall puts the stock reader back. Run the installer again and choose E-Reader to restore this one.

The stock reader that was on the card the first time is kept in the installer's backup folder (`spruce-patch-backup-<date>` in your user profile).

## Credits and licenses

The program is GPL. See `LICENSE`. It is based on [Pixel Reader](https://github.com/ealang/pixel-reader) by ealang. The spruce color theme comes from the [Sundownersport fork](https://github.com/Sundownersport/pixel-reader).

The English dictionary in `resources/dictionary/english.txt` is generated from WordNet 3.1. Princeton's notice is in `resources/dictionary/LICENSE.txt`.

The French and Portuguese dictionaries are generated from Wiktionary data extracted by [Kaikki](https://kaikki.org) (wiktextract). Wiktionary text is available under CC BY-SA 4.0 and the GFDL. The notice is in `resources/dictionary/LICENSE-wiktionary.txt`.

The hyphenation patterns in `resources/hyphenation` are from the [TeX hyph-utf8](https://github.com/hyphenation/tex-hyphen) project, under their own licenses (the English ones are permissive, French is MIT, Portuguese is BSD 3-clause). The notices are in `resources/hyphenation/LICENSE-hyphenation.txt`.

Picture decoding uses [stb_image](https://github.com/nothings/stb), public domain, in `src/extern/stb/stb_image.h`.
