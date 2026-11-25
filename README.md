# Spruce E-Reader

An ebook reader for the Miyoo Flip on spruceOS. It opens epub, FB2, HTML, Markdown and text books, follows the book's own type sizes, shows pictures, and can look a word up in an offline English, French, or Portuguese dictionary, highlight it, bookmark places, search the book, keep a word list and reading stats, hyphenate, remember favorites and finished books, and show footnotes.

On spruce the app is **E-Reader**. This build replaces the stock Pixel Reader binary. Your books, reading position, and highlights stay on the card.

## Put a book on the card

Copy `.epub`, `.fb2`, `.html`, `.md` or `.txt` files to `Roms/EBOOKS` on the SD card. Open **E-Reader**, pick the folder if it is not already there, and press **A** on the book.

The reader remembers the folder and the position in the book. The next time you open that book, it returns to the same place.

## Other book formats

Besides epub, the reader opens:

- **FB2** (FictionBook): paragraphs, titles, emphasis, poems and epigraphs. Titles become the chapters, and the title, author, language and series are read from the file. Pictures inside an FB2 are not shown.
- **HTML** (`.html`, `.htm`, `.xhtml`): the text of the page, with headings, lists, quotes, preformatted text and bold and italic. Headings become the chapters. Scripts, styles and pictures are left out. The `<title>` and the author and language `<meta>` tags are read.
- **Markdown** (`.md`, `.markdown`): headings, paragraphs, bold and italic, lists, quotes, code blocks and rules. A link shows its text. Headings become the chapters and the first heading is the title.
- **Text** (`.txt`): every line is a paragraph, as before. Text in UTF-8, UTF-16 (with a byte order mark) and Latin-1 or Windows-1252 is read correctly: a file that is not valid UTF-8 is taken as Latin-1.

These books work with everything else: bookmarks, highlights, the dictionary, search, per-book layout, favorites and the book list.

## The book list

The list opens on the folder you used last, or on `Roms/EBOOKS` the first time. It stays on the SD card: **Up one folder** stops at the card's top folder, and a saved folder that is not on the card is ignored. The top bar names the folder and shows your place in it, such as `3 / 12`. Each epub is shown by the title and author written inside the book, such as `Dom Casmurro - Machado de Assis`, and a book without them by its file name without the extension. The titles are read in the background a few books at a time and kept in an `info` folder in the reader's store, so the list fills in quickly the first time and at once after that. The type and size are on the right, such as `EPUB, 1.2 MB`; a book you have opened also shows how far you got there, such as `42%, EPUB, 1.2 MB`, or `Done` at 99% and over. Folders sit above the books and say `Folder`. **Up one folder** is the first row inside a subfolder. If a folder has no books or folders, the list says so and tells you where to put books.

A name that is too long for its row slides once the row is selected: it waits a moment, scrolls to the end, pauses, and starts over. In the grid it slides under the selected cover.

**Select** switches between the list and the cover grid. The choice is saved. The grid shows one to four rows of covers, depending on the screen height, with the title under each cover. A book's cover is taken from the book itself (EPUB only) and shrunk in the background, so the grid never waits on it. The small pictures are kept in a `covers` folder inside the reader's store, so they appear at once the next time. A book with no cover, or a `.txt` file, gets a plain colored cover with its title and type. Folders and **Up one folder** get their own tiles.

**Y** opens a small menu for the selected entry:

- **Properties** shows the name, title, author, series, language, type, size, and the folder it is in. Any button closes it.
- **Add to favorites** or **Remove from favorites**, for a book. A favorite has `* ` in front of its name. Favorites in other folders are listed at the top of every folder, under **Continue**, so they are one press away.
- **Mark as finished** or **Mark as not finished**, for a book. A book at 99% or more counts as finished by itself, shows **Done**, and has a **Done** label on its cover. Marking it as not finished overrides that, and marking an unfinished book as finished overrides the percent.
- **Hide finished books** or **Show finished books**, for the whole list. Hidden books stay on the card and come back when you show them. A favorite is never hidden. The choice is saved.
- **Recent books** lists the last 40 books you opened, newest first, with the percent or Done and the date. **A** opens one.
- **Collections** are shelves of your own. The list shows each collection and **New collection...**; **A** opens a collection as a list of its books, and the Y menu there has **Remove from collection**, **Delete collection** and **Leave collection**. **Add to collection** in the Y menu puts the selected book in one. Collections are kept in `collections.tsv` in the store, and follow a book when you rename or delete it.
- In the list, a book you have opened shows the time left to read at your speed, and an unopened text, Markdown, HTML or FB2 book shows an estimate such as `~2h 10m`. Properties has a **Reading time** row. An unopened epub shows none, because its length is unknown until it is opened.
- **Search library** opens an on-screen keyboard. The words you type are looked for in the file name, title, author and series of every book under the top folder, in all its subfolders. A **Searching library** screen with a bar shows how far it has got. The matches replace the list, with the title and the folder they are in; **A** opens one, and **Clear search** (the first row, **B**, or the same entry in the Y menu) goes back to the folders. A search shows up to 500 books.
- **Rename file**, for a book, asks for a new name with the keyboard. The extension stays, and the progress, favorite mark, highlights and bookmarks go with the book.
- **Delete book**, for a book, asks you to confirm and then removes the file together with its progress, highlights, bookmarks and layout.

The **on-screen keyboard** has five pages: lower case, capitals, accented lower case, accented capitals, and symbols. The D-pad moves over the keys, **A** types one, **B** deletes, **Y** types a space, **Select** (or **L1** and **R1**) changes the page, **Start** accepts, and **X** cancels.

A book you have opened has a progress bar along the bottom of its cover. In every folder except the one holding the book you read last, the first row is **Continue: <title>**, which opens that book.

**Start** changes the order of the books: by file name (the default), by title, by author, by series (series name, then the number in the series), or most recently read first. Folders always stay first, and the order is saved. The folder name in the top bar says which order is active.

| Button | In the book list and the grid |
| --- | --- |
| D-pad up / down | List: move one row. Grid: move one row of covers. Hold to keep moving. |
| D-pad left / right | List: move half a screen. Grid: move one cover. |
| L1 / R1 | Move half a screen in the list, or a full screen of covers in the grid. Settings can move this to L2 / R2. |
| A | Open the book or folder. |
| Y | The options of the selected entry: properties, favorite, finished, hide finished, search, rename, delete. |
| Select | Switch between the list and the grid. |
| Start | Change the order: name, title, author, series, recent. |
| X | Settings. |
| B | Go up one folder. In the top folder it does nothing. |
| Menu | Quit the app. |

## Reading

These are the printed buttons on the Flip.

| Button | While reading |
| --- | --- |
| D-pad up / down | Previous or next line. Hold to keep moving. |
| D-pad left / right | Previous or next page. |
| L1 / R1 | Previous or next page. Settings can move this to L2 / R2. The other shoulder pair (L2 / R2, or L1 / R1 after the swap) jumps to the previous or next chapter: the first press goes to the start of the current chapter, and a second one, or one within the first 3% of a chapter, to the one before. |
| A | Show or hide the progress line. |
| B | Close the book and return to the file list. |
| X | Settings. |
| Y | Select a word near the middle of the page. Commas, periods, and quotes are left outside the selection. |
| Select | The book menu: chapters, bookmarks, next and previous mark, book map, go to a percent, go to a page, view pictures, jump back, look up a word, reading stats, word list, highlights. Press Select again to close it. |
| Start | Open the controls guide. Up and down scroll it. Any other button closes it. |
| Menu | Quit the app. A tap of Menu quits. A Menu chord, such as the brightness shortcut, does not. |

The progress line shows the chapter name and how far you are.

**Scroll** is the normal way to read. Up and down move a line, and left and right move a screen. The progress line shows the percentage, because a page number does not belong to a continuous scroll. The percentage follows the text and does not jump when you turn a screen.

**Page** counts real screens from the start of the book or chapter. The progress line shows `12 / 248` plus the percent. When you switch to Page, the reader finishes that count and then jumps to the top of the page you were on, so you are never left in the middle of a page. After that, the D-pad and the shoulder buttons turn a whole page. Up and down do not scroll a line. Each turn adds 1. A new chapter, a title page, and a picture that does not fit the rest of the page start at the top of the next page. The blank line between two paragraphs never opens a page, so a page always starts on text. While the pages are counted, a **Counting pages** screen with a progress bar replaces the text, so nothing is drawn half done and no button turns a page that is not counted yet. **B** still leaves the book. When the count finishes, the book appears at the top of the page you were on. Changing the font counts again and lines you up the same way.

**Book** or **Chapter** applies to both. Book is the whole file. Chapter is the current chapter, so the percentage and the page count both start over at each chapter. In Chapter mode a page turn stops at the edge of the chapter. Turning past the last page of a chapter moves to the first line after it, and turning back from the first page goes to the last page of the previous chapter, so you are never stuck inside one chapter. The new chapter is counted before the page is drawn, so the screen shows the finished page and does not jump.

## Word lookup and highlights

Press **Y** on a page. A cursor sits on a word near the middle of the page, and a bar along the bottom names the buttons. A comma, period, quote, or similar mark attached to that word is not part of the selection.

| Button | While words are selected |
| --- | --- |
| D-pad | Move the cursor. Hold to repeat. Left at the start of the page, or right at the end, turns the page. Until you extend, each move is still one word. |
| Start | Keep the first word and add every word you move through. Press Start again to stop extending. The next move returns to one word. |
| A | Open a small menu over the page, with **Look up**, **Search book**, **Highlight**, **Underline**, and **Strike out**. Look up and Search use the word at the cursor. The three marks cover the whole selection. If that stretch is already marked, the last rows are replaced by **Remove highlight**. **B** returns to the word. |
| B or Y | Leave word selection. The shoulders still turn the page. |

**Look up** opens the definition full screen. Up and down scroll a long definition. Any other button closes it.

The dictionary follows the language the book declares (`dc:language`): `en`, `fr`, or `pt`, with any region such as `pt-BR`. A book in another language, a `.txt` file, or a language whose dictionary file is not on the card uses the English one.

- **English** is WordNet 3.1, one gloss per word, the first sense that was kept.
- **French** and **Portuguese** come from the English Wiktionary through [Kaikki](https://kaikki.org). They give an English gloss per word, up to three part-of-speech readings, and they include inflected forms, such as `plural of livre`. Neither dictionary knows phrases.

Lookup tries the word, then the word without `'s`, then the word without a trailing `s`, then the part after an apostrophe (`l'homme` becomes `homme`). Accented letters are kept, and a capital accented letter matches its lowercase form. If the dictionary file is missing, Look up says there is no entry. A dictionary is read from the card the first time you look something up in it, and a **Loading dictionary** screen with a bar shows that, so the screen never just freezes.

The first time a book is opened, the reader has to measure every part of it. An **Opening book** screen with a bar shows that. The result is remembered, so the next opening is quick.

On a word that is a link in the book, such as a footnote number, the **A** menu also has **Show note**, which opens the text the link points to when it names a spot in the book, and **Go to link**, which jumps there. **Jump back** in the book menu returns.

**Search book** finds the word everywhere in the book, ignoring capitals and matching whole words, and lists the places with the words around each one and their place as a percent. **A** goes there and **Jump back** returns. The list has one row for each paragraph that holds the word, up to 300 rows. The search reads the whole book, so a **Searching** screen with a bar shows how far it has got. If the word is split between two lines with a hyphen, Look up and Search use the whole word.

Every word you look up is added to the **Word list**, newest first, with its meaning and the book it came from. Looking a word up again moves it to the top. **A** shows the meaning, **X** deletes a word. **Export word list** in the book menu writes them to `Word list.txt` in the folder of the book. The list is kept in `vocabulary.tsv` in the store and is shared by all books.

**Highlight** marks the selected words and saves them with the book, along with the words themselves. A highlight is a bar behind each word. **Underline** draws a line under the words instead, and **Strike out** draws one through them. Each mark remembers its own style. Punctuation between the words is not painted. It is still there after you close the book. Removing it deletes the marks that overlap the selection. Installing the reader again does not delete highlights.

The **Highlights** row of the book menu lists every highlight in book order with its place as a percent. **A** goes there and **X** removes one. **Export highlights** writes them to `<title> - highlights.txt` in the folder of the book, which the book list can open. A highlight made before this version has no saved words and shows as `(text not saved)`.

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

## The book menu

**Select** opens a small menu over the page. **B** or **Select** closes it.

- **Chapters** is the book's table of contents, opened on the chapter you are reading. A mark stays on that chapter while you move. Up and down move, **A** jumps to the chapter, **B** or **Select** closes the list. A book with no table of contents does not show this row. There is no separate next-chapter button.
- **Add bookmark** saves the place under the name of its chapter. **Bookmarks** lists them with their place as a percent, **A** goes there, **X** deletes one. They are saved with the book.
- **Next mark** and **Previous mark** go to the next or previous bookmark or highlight in book order, from where you are. They appear when the book has any, and say so when there is none further on.
- **Book map** shows the whole book as one bar: the part you have read is filled, thin lines cut the bar at each chapter start, small squares above it are bookmarks and below it are highlights, and a tall marker is the place the map points at. Left and right move it by 1%, **L1** and **R1** by 10%, up and down jump to the previous or next chapter, and the chapter name is shown below the bar. **A** goes there, **B** closes the map.
- **View picture** or **View pictures** appears when a picture is on the screen. It opens the picture full screen, or a short list to choose from when there are several. **A** zooms in step by step (100%, 150%, 200%, 300%, 400% of the size that fits the screen) and then returns, **L1** and **R1** zoom out and in, **Y** fits the picture to the screen, the D-pad moves around a zoomed picture (hold to keep moving), and **B** closes it.
- **Look up a word** opens the on-screen keyboard. Type a word and the dictionary entry opens. When a selected phrase of several words has no entry as a whole, Look up shows the entry of each word in turn (up to six).
- **Go to %** is a small picker, starting at where you are. Left and right move 1, up and down move 10, **A** goes, **B** cancels. The percent is of the whole book.
- **Go to page** appears in Page mode once the pages are counted. It works like Go to %, starting at the current page, and goes to the top of the chosen page. The page count follows the Progress row, so in Chapter mode the pages are the chapter's.
- **Jump back** appears once you have used the chapter list, a bookmark, a highlight, a link, Go to %, or Go to page. It returns to where you were, and a second use goes one jump further back, up to 20.
- **Reading stats** shows the time spent in this book today and in total, the time in all books today, in the last seven days and in total, and how many books you have started and finished. Time is counted between key presses, and a pause of four minutes or more counts as being away. The numbers are kept in `stats.log` in the store.
- **Word list**, **Export word list**, **Highlights** and **Export highlights** are described above.

## Settings

Press **X**. The book stays behind the menu. Each row has the name on the left and the choice on the right. Up and down move. Left and right change the choice. **B** closes settings. The choices are saved.

- **Theme.** The menu shows Spruce, Night, Light, Sepia, and Vampire. Spruce is the dark theme with cream text.
- **Font size.** Body text, from 18 to 40. Titles scale with it.
- **Font.** The fonts shipped in `resources/fonts`, including DejaVu Sans, DejaVu Serif, and Merriweather. Merriweather uses its bold file for bold text. The other fonts use a synthetic bold.
- **Shoulders.** `L1 / R1` uses L1 and R1 to turn pages. `L2 / R2` uses L2 and R2 instead.
- **Progress.** `Book` or `Chapter`. The percentage, and the page count in Page mode, both follow that choice.
- **Reading.** `Scroll` shows the percentage. `Page` shows a real page count that moves one number at a time.
- **Bar left, Bar middle, Bar right.** What the line under the text shows in each of its three places. The choices are Nothing, Chapter title, Book title, Page and %, Page, Chapter %, Book %, Time, Date, Chapter time left, Book time left, and Battery. Book title is the title inside the book when it has one. The time left items, such as `Ch 12m` and `Book 3h 05m`, come from how fast you have been turning pages: the reader keeps a running average, ignores pauses of four minutes or more and big jumps, and remembers it between sessions. Until it has seen some reading it assumes about 250 words a minute. Battery is read from the system and the item stays empty when the device does not report it. Page and % is the usual 12 / 248  34%, and its percent follows the Progress row. Chapter % and Book % always show their own number, so they can sit side by side. The line starts as Chapter title on the left and Page and % on the right. The right place keeps what it needs, the middle place comes next, and the left place gets what is left and is cut short if it has to be.
- **Clock.** 24-hour or 12-hour, for the time in the bar. The time and date are the ones the device keeps, and the line redraws when the minute changes.
- **Margins.** The empty space at each side of the text: Narrow (the default), Medium, Wide, or Extra wide.
- **Line spacing.** Tight, Normal (the default), Relaxed, or Loose. It changes the height of every line.
- **Indent.** None (the default), Small, Medium or Large: the space before the first line of each paragraph.
- **Paragraph gap.** On (the default) keeps the empty line between paragraphs. Off removes it, which looks best together with an indent. Changing these counts the pages again.
- **Bold text.** Draws all body text in bold. Off is the default.
- **Word spacing.** Tight, Normal (the default), Wide or Wider: extra pixels in every space between words. Counts the pages again.
- **Scroll overlap.** 0 to 3 lines of the old screen stay at the top when left or right scrolls a screen in Scroll mode, so you do not lose your place.
- **Pictures.** Normal, Inverted, High contrast or Hidden. Inverted and High contrast are for dark themes and for line art. Hidden removes pictures from the text.
- **Thin bar.** On draws a 3 pixel line along the bottom edge: the part read is filled and thin ticks mark the chapters.
- **Cover screen.** Off, or 2, 5 or 10 minutes. When no button has been pressed for that long, the cover of the open epub fills the screen; any button brings the book back. Books without a cover picture do not show one.
- **Night theme, Night from, Night until.** Night theme is Off or any of the themes. When it is set, the reader switches to it from the hour in Night from until the hour in Night until (for example 21:00 to 07:00), using the clock of the device, and goes back to your own theme afterwards. A theme you pick by hand during the night is kept.
- **Hyphenate.** On splits a word that does not fit at the end of a line, with a hyphen, so lines are fuller and justified lines have smaller gaps. The patterns for English, French and Portuguese follow the language of the book, and a book in another language uses the English ones. The hyphen is not part of the book, so highlights and bookmarks stay in the same place with Hyphenate on or off. Off is the default. Changing it counts the pages again.
- **Auto exit.** Off, or 5, 10, 15, 30 or 60 minutes. When no button has been pressed for that long, the reader saves its place and quits, so a book left open does not run the battery down.
- **Justify.** On spreads the words of each line so it fills the width, except the last line of a paragraph, headings, centered text, and preformatted text. A line that falls well short of full is left alone. Off is the default.
- **Controls.** Press **A** to open the same shortcut guide that **Start** opens while reading.

Margins and line spacing re-wrap the book, so the page count is made again, as it is for the font size.

Margins, Line spacing, Justify, Indent, Paragraph gap, Bold text, Hyphenate and Reading are kept for each book. Open Settings while a book is open and the change belongs to that book only, and the next time you open it the same layout comes back. Open Settings in the book list and the change sets the defaults every other book starts with. A book you never changed uses the defaults. The layout of a book is in a `.layout` file beside its highlights.

**A** while reading shows or hides the progress line. That choice is saved too.

## What is saved

The reader writes under `App/PixelReader/.pixel_reader_store`.

- Reading position and the last folder you browsed.
- Theme, font, font size, shoulder buttons, whether the progress line is shown, the bar, clock, margins, line spacing, justify, the book list order, and your reading speed.
- Highlights, bookmarks and the layout, one file for each of them per book, named from the book file.
- `stats.log` and `vocabulary.tsv`: the reading time for each day and book, and the words you looked up.
- `library.log`: for each book you opened, how far you got and when, whether it is a favorite or marked finished, and the name of its saved data. The list uses it for progress, sorting by recent, Continue, favorites and finished.
- `info` and `covers`: the titles and the cover pictures read from your books. They can be deleted at any time and are made again.

Books themselves are not copied or modified. A later install of this reader replaces the program and the dictionary, and leaves the store alone.

## Install on spruce

The Flip build is installed by [spruce-patch](https://github.com/mucadoo/spruce-patch). On Windows, with the spruce card plugged in, run that installer's `install.bat` and choose **E-Reader**. It copies:

- `App/PixelReader/reader`
- `App/PixelReader/resources/dictionary/english.txt`, `french.txt`, and `portuguese.txt`
- `App/PixelReader/resources/hyphenation/english.txt`, `french.txt`, and `portuguese.txt`, with their notice
- the WordNet notice and the Wiktionary notice next to those dictionaries

Quit E-Reader if it is open, eject the card, and open E-Reader again. The new reader is used the next time the app starts. A spruce reinstall puts the stock reader back. Run the installer again and choose E-Reader to restore this one.

The stock reader that was on the card the first time is kept in the installer's backup folder (`spruce-patch-backup-<date>` in your user profile).

## Building

The Flip binary is aarch64, dynamically linked, and needs glibc 2.17. It uses the device's SDL 1.2, SDL_ttf, SDL_image, libzip, and libxml2. Jpeg, png, and gif pictures are decoded inside the reader, so a firmware SDL_image without jpeg still shows them. C++ is linked statically.

The source layout matches [Pixel Reader](https://github.com/ealang/pixel-reader). `make` builds the desktop and test targets. The Flip binary is cross-built from this tree for `aarch64-linux-gnu` with glibc 2.17.

## Credits and licenses

The program is GPL. See `LICENSE`. It is based on [Pixel Reader](https://github.com/ealang/pixel-reader) by ealang. The spruce color theme comes from the [Sundownersport fork](https://github.com/Sundownersport/pixel-reader).

The English dictionary in `resources/dictionary/english.txt` is generated from WordNet 3.1. Princeton's notice is in `resources/dictionary/LICENSE.txt`.

The French and Portuguese dictionaries are generated from Wiktionary data extracted by [Kaikki](https://kaikki.org) (wiktextract). Wiktionary text is available under CC BY-SA 4.0 and the GFDL. The notice is in `resources/dictionary/LICENSE-wiktionary.txt`.

The hyphenation patterns in `resources/hyphenation` are from the [TeX hyph-utf8](https://github.com/hyphenation/tex-hyphen) project, under their own licenses (the English ones are permissive, French is MIT, Portuguese is BSD 3-clause). The notices are in `resources/hyphenation/LICENSE-hyphenation.txt`.

Picture decoding uses [stb_image](https://github.com/nothings/stb), public domain, in `src/extern/stb/stb_image.h`.
