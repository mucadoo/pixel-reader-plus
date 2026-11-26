# Spruce E-Reader

An ebook reader for the Miyoo Flip on spruceOS. It opens epub, Markdown and text books, follows the book's own type sizes, shows pictures, and can look a word up in an offline English, French, or Portuguese dictionary, highlight it, bookmark places, search the book, keep a word list and reading stats, hyphenate, remember favorites and finished books, and show footnotes.

On spruce the app is **E-Reader**. This build replaces the stock Pixel Reader binary. Your books, reading position, and highlights stay on the card.

## Put a book on the card

Copy `.epub`, `.md` or `.txt` files to `Roms/EBOOKS` on the SD card. Open **E-Reader**, pick the folder if it is not already there, and press **A** on the book.

The reader remembers the folder and the position in the book. The next time you open that book, it returns to the same place.

## Other book formats

Besides epub, the reader opens Markdown and plain text. Other types (HTML, FB2, comics and so on) are not opened.

- **Markdown** (`.md`, `.markdown`): headings, paragraphs, bold and italic, lists, quotes, code blocks and rules. A link shows its text. Headings become the chapters and the first heading is the title.
- **Text** (`.txt`): every line is a paragraph, as before. Text in UTF-8, UTF-16 (with a byte order mark) and Latin-1 or Windows-1252 is read correctly: a file that is not valid UTF-8 is taken as Latin-1.

These books work with everything else: bookmarks, highlights, the dictionary, search, per-book layout, favorites and the book list.

## The book list

The list opens on the folder you used last, or on `Roms/EBOOKS` the first time. It stays on the SD card: **Up one folder** stops at the card's top folder, and a saved folder that is not on the card is ignored. The top bar names the folder and shows your place in it, such as `3 / 12`. Each epub is shown by the title and author written inside the book, such as `Dom Casmurro - Machado de Assis`, and a book without them by its file name without the extension. The titles are read in the background a few books at a time and kept in an `info` folder in the reader's store, so the list fills in quickly the first time and at once after that. The type and size are on the right, such as `EPUB, 1.2 MB`; a book you have opened also shows how far you got there, such as `42%, EPUB, 1.2 MB`, or `Done` at 99% and over. Folders sit above the books and say `Folder`. **Up one folder** is the first row inside a subfolder. If a folder has no books or folders, the list says so and tells you where to put books.

A name that is too long for its row slides once the row is selected: it waits a moment, scrolls to the end, pauses, and starts over. In the grid it slides under the selected cover.

**Select** switches between the list and the cover grid. The choice is saved. The grid shows one to four rows of covers, depending on the screen height, with the title under each cover. A book's cover is taken from the book itself (EPUB only) and shrunk in the background, so the grid never waits on it. The small pictures are kept in a `covers` folder inside the reader's store, so they appear at once the next time. A book with no cover, or a `.txt` file, gets a plain colored cover with its title and type. Folders and **Up one folder** get their own tiles.

**Y** opens a small menu for the selected entry:

- **Properties** opens a page about the book: the cover, then title, author, series, language, publisher and date (epub), type, size, status, progress, reading time, the date of the file, its name and folder, and the book's own description when it has one. Up and down scroll, left and right turn a page, any other button closes it.
- **Add to favorites** or **Remove from favorites**, for a book. A favorite has `* ` in front of its name. Favorites in other folders are listed at the top of every folder, under **Continue**, so they are one press away.
- A book at 99% or more counts as finished by itself, shows **Done**, and has a **Done** label on its cover. **Status** (below) can override that either way.
- **Hide finished books** or **Show finished books**, for the whole list. Hidden books stay on the card and come back when you show them. A favorite is never hidden. The choice is saved.
- **Recent books** lists the last 40 books you opened, newest first, with the percent or Done and the date. **A** opens one.
- **Collections** are shelves of your own. The list shows each collection and **New collection...**; **A** opens a collection as a list of its books, and the Y menu there has **Remove from collection**, **Delete collection** and **Leave collection**. **Add to collection** in the Y menu puts the selected book in one. Collections are kept in `collections.tsv` in the store, and follow a book when you rename or delete it.
- In the list, a book you have opened shows the time left to read at your speed, and an unopened text or Markdown book shows an estimate such as `~2h 10m`. Properties has a **Reading time** row. An unopened epub shows none, because its length is unknown until it is opened.
- **Status** sets a book to Automatic (by progress), To read, Reading, On hold or Finished; it replaces the old finished mark. A book you marked To read or On hold shows that in the list. **Library tools** is grouped into **Browse**, **Clean up**, **Get books** and **Back up**, with **Search inside all books** on its first screen, and holds: **Books by status** (To read, Reading, On hold, Finished and Favorites, each as a list of books), **Find copies** (books with the same name apart from case and punctuation, or the same file type and exact size, listed together so you can delete the extra one), **Books not opened yet**, **Browse by series**, **Browse by author** and **Browse by language** and **Browse by tag or genre** (the books grouped by what the book says about itself, with the number of books in each group; a series lists its books in the order of their numbers, and books that say nothing are in a last group called (none)), **Reading history** (a calendar of the month, see below), **Tidy this folder into author folders** (the loose books of the folder are moved into a folder for each author, or each author and then each series; you first see how many books go into how many folders and can cancel; books that name no author stay; reading place, status and collections move along), **Search inside all books** (type a word or phrase and the reader looks through every epub, Markdown and text book of the library, a progress bar showing the book it is in, and lists the places with the words around them and the book title under each, up to 6 places for a book and 300 in all; **A** opens that book at that place; a big library takes a while), **All highlights** (every highlight of every book you opened, newest book first, with the book under each; **Search...** on the first row narrows them by words in the highlight or the book title, and **A** opens the book at the place of the highlight; only highlights with their words kept are listed, which is every one made since the words were saved), **Back up settings and notes**, **Restore from backup** and **Get books online (OPDS)** and **Receive books over wifi**. **Receive books over wifi** starts a small web page on the handheld and shows its address, such as `http://192.168.1.50:8080`. Open it in a browser on a phone or computer on the same wifi, choose files and send them; they are saved in the **Downloads** folder of the library. Anyone on the same wifi can send files while the screen is open (only epub, Markdown and text files are kept), and it stops when you press **B**. **Get books online (OPDS)** reads an OPDS catalog (the format Calibre-web, Kavita, COPS and many free libraries serve) over wifi. It asks for the catalog address once, such as `http://192.168.1.20:8083/opds` (add `name:password@` before the host for a server that asks for a login), and keeps it. Folders of the catalog are marked `>`, books show their file type, and choosing a book saves it in a **Downloads** folder of the library; **More...** goes to the next page and **B** goes back a level. Only plain `http://` works, since the reader has no encryption, and only epub, Markdown and text files are offered. Both network parts were tested against test servers on a computer, not yet on the handheld.
- **Back up settings and notes** writes `PixelReader backup.zip` in the books folder: the settings, your place in every book, the library history and statuses, collections, the word list, reading stats, and the bookmarks, highlights and layout of every book. Covers, book info and page counts are not included, because they are made again. **Restore from backup** puts the files back and then closes the reader, so open it again afterwards. Restoring replaces what is on the card now.
- **Search library** opens an on-screen keyboard. The words you type are looked for in the file name, title, author and series of every book under the top folder, in all its subfolders. A **Searching library** screen with a bar shows how far it has got. The matches replace the list, with the title and the folder they are in; **A** opens one, and **Clear search** (the first row, **B**, or the same entry in the Y menu) goes back to the folders. A search shows up to 500 books.
- **Rename file**, for a book, asks for a new name with the keyboard. The extension stays, and the progress, favorite mark, highlights and bookmarks go with the book.
- **Delete book**, for a book, asks you to confirm and then removes the file together with its progress, highlights, bookmarks and layout.
- **File management.** **Mark for copy, move or delete** puts `[x] ` in front of a book (choose **Unmark** to take it off); mark as many as you like, in one folder or several. Then **Copy marked**, **Move marked** (or **Copy this book** / **Move this book** with nothing marked) remember the books, you open the folder you want and choose **Paste here** in the Y menu. A name that is already taken gets ` (2)` added. A moved book keeps its progress, favorite mark, status, highlights, bookmarks and collections. **Delete marked** asks once and removes all of them, and **Clear marks** unmarks everything. **New folder** asks for a name with the keyboard and makes the folder in the folder you are in.
- **Next in series.** When the book you read last is finished, the list shows **Next in series: <title>** under **Continue**, if the folder holds the next number of its series. At the end of a book, pressing forward once more on the last page opens a small menu: **Next in series** (the book with the next series number in the same folder, else **Next in folder**, the next file in natural name order, where `Book 2` comes before `Book 10`), **Back to the list**, or **Stay here**.

The **on-screen keyboard** has five pages: lower case, capitals, accented lower case, accented capitals, and symbols. The D-pad moves over the keys, **A** types one, **B** deletes, **Y** types a space, **Select** (or **L1** and **R1**) changes the page, **Start** accepts, and **X** cancels.

A book you have opened has a progress bar along the bottom of its cover. In every folder except the one holding the book you read last, the first row is **Continue: <title>**, which opens that book.

**Each folder keeps its own view.** Changing the order (Start), list or grid (Select) or hide-finished (Y menu) in a subfolder is remembered for that folder only, in `folders.tsv` in the store. The top folder sets the default for folders that have no choice of their own, as before. **Use the default view in this folder** in the Y menu clears a folder's choice.

**Start** changes the order of the books: by file name (the default), by title, by author, by series (series name, then the number in the series), most recently read first, largest first, newest file first (the date the file was put on the card), or most read first (books in progress, the furthest along first, then books not opened, then finished ones). Folders always stay first, and the order is saved. The folder name in the top bar says which order is active.

| Button | In the book list and the grid |
| --- | --- |
| D-pad up / down | List: move one row. Grid: move one row of covers. Hold to keep moving. |
| D-pad left / right | List: move half a screen. Grid: move one cover. |
| L1 / R1 | Move half a screen in the list, or a full screen of covers in the grid. Settings can move this to L2 / R2. |
| A | Open the book or folder. |
| Y | The options of the selected entry: properties, favorite, finished, hide finished, **Show** (switches which books the folder lists: all, not opened, in progress, favorites, finished), search, rename, delete. |
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
| Select | The book menu: chapters, search in book, bookmarks, next and previous mark, book map, skim the book, go to a percent, go to a page, view pictures, jump back, look up a word, reading stats, word list, highlights. Press Select again to close it. |
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
| X | Grow the selection: first to the whole sentence (up to a full stop, question or exclamation mark), then to the whole paragraph, then back to the word you started from. It works on the words on the page. Then **A** can look the phrase up or highlight all of it. |
| A | Open a small menu over the page, with **Look up**, **Look up in...** (when there is more than one dictionary), **Search book**, **Highlight**, **Underline**, and **Strike out**. Look up and Search use the word at the cursor. The three marks cover the whole selection. If that stretch is already marked, the last rows are replaced by **Remove highlight**. **B** returns to the word. |
| B or Y | Leave word selection. The shoulders still turn the page. |

**Look up** opens the definition full screen. Up and down scroll a long definition. Any other button closes it.

The dictionary follows the language the book declares (`dc:language`): `en`, `fr`, or `pt`, with any region such as `pt-BR`. A book in another language, a `.txt` file, or a language whose dictionary file is not on the card uses the English one.

- **English** is WordNet 3.1, one gloss per word, the first sense that was kept.
- **French** and **Portuguese** come from the English Wiktionary through [Kaikki](https://kaikki.org). They give an English gloss per word, up to three part-of-speech readings, and they include inflected forms, such as `plural of livre`. Neither dictionary knows phrases.

Lookup tries the word, then the word without `'s`, then the word without a trailing `s`, then the part after an apostrophe (`l'homme` becomes `homme`). Accented letters are kept, and a capital accented letter matches its lowercase form. If the dictionary file is missing, Look up says there is no entry. A dictionary is read from the card the first time you look something up in it, and a **Loading dictionary** screen with a bar shows that, so the screen never just freezes.

The first time a book is opened, the reader has to measure every part of it. An **Opening book** screen with a bar shows that. The result is remembered, so the next opening is quick.

On a word that is a link in the book, such as a footnote number, the **A** menu also has **Show note**, which opens the text the link points to when it names a spot in the book, and **Go to link**, which jumps there. **Jump back** in the book menu returns.

**Search book** finds the word everywhere in the book and lists the places with the words around each one, their place as a percent and the chapter. **Search in book** in the book menu (**Select**) takes any word or phrase and keeps your last eight searches; it has four switches, **Whole words** (on by default), **Match case** (off by default), **Match accents** (off by default, so a search for `cafe` also finds `café`) and **This chapter only** (off by default), kept between sessions. The word menu's **Search book** uses the same switches. **A** goes there and **Jump back** returns. The list has one row for each paragraph that holds the word, up to 300 rows. The search reads the whole book, so a **Searching** screen with a bar shows how far it has got. If the word is split between two lines with a hyphen, Look up and Search use the whole word.

Every word you look up is added to the **Word list**, newest first, with its meaning and the book it came from. Looking a word up again moves it to the top. **A** shows the meaning, **X** deletes a word. **Export word list** in the book menu writes them to `Word list.txt` in the folder of the book. The list is kept in `vocabulary.tsv` in the store and is shared by all books.

New highlights use the color chosen in Settings (**Highlight color**: Yellow, Green, Blue or Pink), and each highlight keeps its own color, so you can change the setting and go on with another color. Colors are tuned for light and dark themes. Exports mark colored ones, such as `[Green]`.

**Highlight** marks the selected words and saves them with the book, along with the words themselves. A highlight is a bar behind each word. **Underline** draws a line under the words instead, and **Strike out** draws one through them. Each mark remembers its own style. Punctuation between the words is not painted. It is still there after you close the book. Removing it deletes the marks that overlap the selection. Installing the reader again does not delete highlights.

The **Highlights** row of the book menu lists every highlight in book order with its place as a percent. **A** goes there, **Y** writes a **note** on the highlight (an empty note removes it; a highlight with a note has a `*` in front) and **X** removes one. Notes are kept with the highlights and also show in the library list **All highlights** in square brackets, where the search finds them. **Export highlights and notes** writes `<title> - highlights.md` in the folder of the book: a Markdown file with the chapter names, each highlight as a quotation, its note, its place and color. A hidden comment after each one holds its exact place, so **Import highlights** (shown in the book menu when that file exists) reads the file back into the book, on this card or another copy of the same book; highlights already there are kept and missing notes are filled in. The format is the reader's own, not KOReader's. **Library tools > Export all highlights and notes** writes `All highlights.md` in the books folder, with every book's highlights and notes. A highlight made before this version has no saved words and shows as `(text not saved)`.

## How the text is shown

The reader reads the book's stylesheet, not only heading tags. A title written as a normal paragraph with a large size is drawn large. A chapter number and a chapter title keep the sizes the book asked for, and centered text stays centered.

- Bold and italic in the book are kept, including italics that the book marks with a class instead of an `<i>` tag.
- A quoted block is indented and italic.
- Preformatted text uses a monospace font.
- A simple table of an epub is set as columns, each column as wide as its longest cell, with every row on one line. A table too wide for the screen, or with merged cells or nested tables, is shown as before, a cell after cell.
- A very large first letter, the kind a book uses as a drop cap, stays large on that line. The lines after it go back to body size.
- Real headings (`h1` to `h6`) are still larger and bold when the book does not give them a size. `h1` and `h2` are centered unless the stylesheet says otherwise.

The size you pick in settings is the body size. Titles are scaled from that, and they are kept between 12 and 72 pixels so a title cannot become unreadably small or larger than the screen.

## Pictures

Jpeg, png, and gif pictures in an epub are drawn in the page. A wide picture is scaled down to the screen width. A tall picture continues on the following lines, so you scroll through it.

The cover, a title-page portrait, and photo inserts are ordinary pictures in the book. If a picture cannot be read, the page shows the picture's path in brackets instead of a blank gap.

## The book menu

**Select** opens a small menu over the page. **B** or **Select** closes it. The first screen holds **Chapters** (when the book has them), **Next match** and **Previous match** while a search is open, and one row for each group; **A** opens a group and **B** goes back to the first screen. The groups are **Go to** (book map, skim the book, go to a percent or page, jump back), **Search**, **Marks** (bookmarks, highlights and stepping between them, exports), **Words** (look up, word list, book language), **Pictures** and **Tools** (screen dim, sync progress, reading history and stats). A group with only one row, such as Search when no search is open, is shown on the first screen instead.

- **Books with no table of contents.** An epub made of one to four big files and without a table of contents gets one from its headings (h1 to h3). A plain text file gets one from lines that look like chapter titles (`Chapter 3`, `Part II`, `Prologue`, or just a number or Roman numeral alone between blank lines), and a long text without any is cut into **Part 1**, **Part 2** and so on, about 30 000 letters each.
- **Chapters** is the book's table of contents (**Y** types a few letters and lists only the chapters with them in the name), opened on the chapter you are reading. A mark stays on that chapter while you move. Up and down move, **A** jumps to the chapter, **B** or **Select** closes the list. Each row says how it stands: **Read** for the chapters behind you, the percent done and the time left for the one you are in, and the reading time of the ones ahead (from your own reading speed). When chapters sit inside others (parts and chapters), **X** folds the list down to the top level and unfolds it again. A book with no table of contents does not show this row. There is no separate next-chapter button.
- **Book language** (in the book menu) picks the language for hyphenation and lookups for this book when the epub names the wrong one or none; **Follow the book** returns to what the book says. It is kept for each book.
- **Search in book** asks for a word or phrase and lists the places (see Word lookup). Once you choose a place, the search stays open: the words are marked on the page, the bar shows `Match 3 / 17` before the chapter title, and the shoulder pair that normally jumps between chapters steps to the next and previous match instead (round the ends). The book menu then also has **Next match**, **Previous match** and **Clear search**, which gives the shoulders back to the chapters. It keeps your last eight searches as rows to pick again, and has the **Whole words**, **Match case**, **Match accents** and **This chapter only** switches. Library search and Search inside all books ignore accents too.
- **Screen dim** in the book menu cycles the Screen dim setting (Off, Dim, Dimmer, Dimmest) without leaving the book. The device's own brightness chords work in the reader too.
- **Add bookmark** saves the place under the name of its chapter; on a page that already has one the row reads **Remove the bookmark on this page**. **Start** does the same in one press while reading: it adds a bookmark, or removes the one on the page. A page that holds a bookmark has a small corner mark at its top right. **Bookmarks** lists them with their place as a percent, **A** goes there, **X** deletes one. They are saved with the book.
- **Next mark** and **Previous mark** go to the next or previous bookmark or highlight in book order, from where you are. They appear when the book has any, and say so when there is none further on.
- **Book map** shows the whole book as one bar: the part you have read is filled, thin lines cut the bar at each chapter start, small squares above it are bookmarks and below it are highlights, and a tall marker is the place the map points at. Left and right move it by 1%, **L1** and **R1** by 10%, up and down jump to the previous or next chapter, and the chapter name is shown below the bar. **A** goes there, **B** closes the map.
- **Pictures in this book** reads the book and shows every picture (up to 400) as a grid of small thumbnails; **A** opens one full screen, **Y** goes to the place in the book where it is, **L** and **R** move a screen, **B** closes.
- **View picture** or **View pictures** appears when a picture is on the screen. It opens the picture full screen, or a short list to choose from when there are several. **A** zooms in step by step (100%, 150%, 200%, 300%, 400% of the size that fits the screen) and then returns, **L1** and **R1** zoom out and in, **Y** fits the picture to the screen, the D-pad moves around a zoomed picture (hold to keep moving), and **B** closes it.
- **Skim the book** shows the book as a grid of 50 tiles, one for every 2%, each with the first words found there and the chapter it is in; the tile of your place is marked with `>`. The D-pad moves, **L1** and **R1** go a screen at a time, **A** goes there.
- **Look up a word** first lists **Type a word...**, **Browse the dictionary...** and the words you looked up lately (up to 20, newest first); choose a word to open its entry again, or type a new one with the on-screen keyboard. **Browse the dictionary...** asks for the start of a word (leave it empty to start at the first word) and lists the words that begin with it, up to 400, in the dictionary of the settings (or the book's); **A** opens the entry of the word. **B** in an entry that you reached by picking a word goes back to the entry before. Type a word and the dictionary entry opens. When a single word has no entry, a **No entry** list offers near matches (other endings, a misspelled letter, a missing or extra letter), and **A** opens the one you pick. When a selected phrase of several words has no entry as a whole, Look up shows the entry of each word in turn (up to six).
- **Go to %** is a small picker, starting at where you are. Left and right move 1, up and down move 10, **A** goes, **B** cancels. The percent is of the whole book.
- **Go to book page** appears for an epub that lists its printed pages (the page-list of its navigation file, which many books made from a paper edition have). Type the page as printed, such as `212` or `xiv`, and the reader goes to where it starts. The bar then shows `p. 212` in place of the counted page in the Page and Progress items, wherever you are in the book. **Ignore the book's page numbers** in Style tweaks switches this off, for the next book you open.
- **Go to page** appears in Page mode once the pages are counted. It works like Go to %, starting at the current page, and goes to the top of the chosen page. The page count follows the Progress row, so in Chapter mode the pages are the chapter's.
- **Jump back** appears once you have used the chapter list, a bookmark, a highlight, a link, Go to %, or Go to page. It returns to where you were, and a second use goes one jump further back, up to 20.
- **Reading history** is a calendar of one month. Each day has a bar as tall as the time you read that day (against your best day of the month), today has an outline, and the day you have chosen lists the books it was spent on with the time in each. The top shows the month, the days you read and the total. Left and right move a day, up and down a week, **L1** and **R1** a month, **B** closes. It is also in Library tools.
- **Reset this book's layout** (under **Tools**) drops the margins, spacing and other layout kept for this book, so it uses the defaults again.
- **Reading stats** shows, for this book, the time today and in total and the reading speed (about so many words a minute, worked out from how far you are and the time spent); for all books the time today, in 7 and 30 days, the average on a day you read, the total and the best day; a chart of the last 14 days; the books you read most; the books finished each month (by the date they were last opened); and how many books you have started and finished. **Library tools > Reading stats** shows the same without the part about one book. Time is counted between key presses, and a pause of four minutes or more counts as being away. The numbers are kept in `stats.log` in the store.
- **Sync progress** keeps your place on a progress server that speaks the KOReader sync protocol (the free KOSync server, or your own). **Server settings...** asks for the server address (http:// only), a user name and a password; **Create account on the server** registers them. Then **Send my place now** and **Get the place from the server** work, and **Sync when opening and leaving** does both by itself: when you open a book that is further along on the server, a menu offers **Go to NN% (device)** or **Stay**. Two readers share a place only by percent, using the file name of the book, so the same file name is needed on both. Not yet tried on the handheld.
- **Word list**, **Export word list**, **Highlights** and **Export highlights** are described above.

## Settings

Press **X**. The book stays behind the menu. The first screen lists groups: **Theme and colors**, **Page layout**, **Text**, **Reading**, **Bar and clock**, **Library and power** and **Controls**; **A** opens one and **B** goes back (and **B** on the first screen closes settings). Each row has the name on the left and the choice on the right. Up and down move. Left and right change the choice. **B** closes settings. The choices are saved.

- **Custom colors.** Opens a list with **Background**, **Text** and **Highlight**: **A** steps each through a palette of 22 colors, and **Preset** applies Solarized light, Solarized dark, Paper, Amber terminal, Green terminal or Blue night in turn. Any change switches to the theme called **Custom**, which is also one of the Theme choices, and the colors are kept. The other colors follow: the quiet text is a blend of text and background, and the text on the highlight is black or white, whichever reads better.
- **Typography.** For each book: **Ligatures** draws ff, fi, fl, ffi and ffl as one letter when the font has it (off by default; search and lookup still see the plain letters), and **Widows and orphans** (page mode) moves a page break up a line so a page does not end on the first line of a paragraph or leave just its last line for the next page. A heavier text is **Bold text**; a lighter weight is not possible with the font library the reader uses.
- **Theme.** The menu shows Spruce, Night, Light, Sepia, Vampire and Custom. Spruce is the dark theme with cream text.
- **Font size.** Body text, from 18 to 40. Titles scale with it. **A** on this row or on **Font** opens a preview: a sample paragraph (with quotes, numbers and accented letters) in the font and size you are looking at. Up and down change the font, left and right the size, **A** keeps both and **B** leaves them as they were.
- **Font.** The fonts shipped in `resources/fonts`, including DejaVu Sans, DejaVu Serif, and Merriweather. Merriweather uses its bold file for bold text. The other fonts use a synthetic bold.
- **Shoulders.** `L1 / R1` uses L1 and R1 to turn pages. `L2 / R2` uses L2 and R2 instead.
- **Progress.** `Book` or `Chapter`. The percentage, and the page count in Page mode, both follow that choice.
- **Reading.** `Scroll` shows the percentage. `Page` shows a real page count that moves one number at a time.
- **Bar left, Bar middle, Bar right.** What the line under the text shows in each of its three places. The choices are Nothing, Chapter title, Book title, Page and %, Page, Chapter %, Book %, Time, Date, Chapter time left, Book time left, Chapter x of y, Pages left in chapter, Pages left in book, and Battery. Chapter x of y shows `Ch 3 / 20`. The pages left items, such as `Ch 12 pg left` and `Book 340 pg left`, are shown in page mode: the count for what the Progress row measures is exact, and the other is estimated from the text that is left. Book title is the title inside the book when it has one. The time left items, such as `Ch 12m` and `Book 3h 05m`, come from how fast you have been turning pages: the reader keeps a running average, ignores pauses of four minutes or more and big jumps, and remembers it between sessions. Until it has seen some reading it assumes about 250 words a minute. Battery is read from the system and the item stays empty when the device does not report it. Page and % is the usual 12 / 248  34%, and its percent follows the Progress row. Chapter % and Book % always show their own number, so they can sit side by side. The line starts as Chapter title on the left and Page and % on the right. The right place keeps what it needs, the middle place comes next, and the left place gets what is left and is cut short if it has to be.
- **Clock.** 24-hour or 12-hour, for the time in the bar. The time and date are the ones the device keeps, and the line redraws when the minute changes.
- **Screen.** Landscape (the default), Portrait or Portrait, flipped: the page is turned on its side so a book fills the handheld held upright. Choose Portrait if you turn the handheld so that its right side is up (the D-pad then sits at the bottom), and Portrait, flipped for the other way around. The D-pad arrows follow the screen, so the arrow that points up on the screen moves up. The face and shoulder buttons keep their usual jobs. When you close the settings the reader restarts by itself, back to the book you had open. Not yet tried on the handheld.
- **Left margin** and **Right margin.** The empty space at each side of the text, set apart: Narrow (the default), Medium, Wide, or Extra wide. A wider left margin leaves room for a thumb or a binding.
- **Top margin** and **Bottom margin.** The empty space above and below the lines: None (the default), Small, Medium or Large. A taller margin means fewer lines on a page, so the page count is made again.
- **Line spacing.** Tight, Normal (the default), Relaxed, or Loose. It changes the height of every line.
- **Indent.** None (the default), Small, Medium or Large: the space before the first line of each paragraph.
- **Paragraph gap.** On (the default) keeps the empty line between paragraphs. Off removes it, which looks best together with an indent. Changing these counts the pages again.
- **Bold text.** Draws all body text in bold. Off is the default.
- **Dictionary.** Auto follows the language of the book. Any other choice is a dictionary on the card, for every book. The dictionaries are the `.txt` files in `resources/dictionary` (English, French and Portuguese come with the reader): one `word<TAB>meaning` line for each word, with the word in lowercase. The file does not have to be sorted. Put your own file there and it appears in this row and in **Look up in...**. **StarDict** dictionaries work too, the format of most free dictionaries: put the dictionary in a folder of its own inside esources/dictionary, with its .ifo, .idx and .dict (or .dict.dz) files, and the folder name appears in the list. The words are read when the dictionary is first used (a big one takes a moment, with a progress bar), the articles from the file as you look words up. Links, pictures and sounds in an article are dropped and its formatting is turned to plain text. The Word list is the history of everything you looked up. **All at once** (and **All dictionaries** in **Look up in...**) looks the word up in every dictionary and shows one page with a section for each dictionary that knows it, headed by its name in brackets. In an entry, **A** starts picking a word: the arrows move from word to word, **A** looks the picked word up in the same dictionaries, and any other button stops picking; **B** closes.
- **Screen dim.** Off, Dim, Dimmer or Dimmest: 25%, 50% or 70% darker, drawn by the reader on top of the backlight, for the dark where the lowest backlight step is still too bright. It does not save battery.
- **Book styles.** On follows the stylesheet of an epub (sizes, bold, italic, centering, page breaks). Off ignores it and uses the reader's own look, which fixes books with a bad stylesheet. It applies from the next time a book is opened.
- **Highlight color.** Yellow, Green, Blue or Pink for new highlights.
- **Fallback fonts.** A letter the chosen font does not have (a symbol, Greek or Cyrillic letters, CJK) is drawn from another font instead of an empty box. The reader looks first in any font file in `resources/fonts` whose name starts with `fallback` (for example `fallback-NotoSansCJK.ttf`; put your own there), then in DejaVu Sans Mono. A font file is only used for letters in the first 65 536 (the basic multilingual plane).
- **Embedded fonts.** An epub that carries its own font (a `@font-face` rule in its stylesheet, TrueType or OpenType, not woff) is read in that font, with its bold and italic faces when it has them, if its stylesheet says which font the text is set in (the body, html or p rule) or it has just the one. The files are copied to the `fonts` folder of the store the first time. **Ignore the book's font** in Style tweaks turns this off for the next book you open. A book font can be taller than the reader's own, which crowds the lines a little.
- **Style tweaks.** Opens switches for how a book's own styling is used, for the next book you open: **Ignore page breaks** (also no new page at each h1), **Ignore font sizes**, **Ignore centered text**, and **Ignore the book's font** and **Ignore the book's page numbers**, and **Curly quotes**, which turns straight quotes into curly ones (the same number of characters, so bookmarks and highlights stay in place). The Book styles row switches the whole stylesheet off instead.
- **Layout profiles.** Saves the page settings (font, size, theme, margins, line spacing, justify, indent, paragraph gap, bold, hyphenation, word and letter spacing, pictures) under a name you type. Pick a saved profile to use it, replace it with the current settings, or delete it. The profiles are kept in profiles.tsv in the store.
- **Word spacing.** Tight, Normal (the default), Wide or Wider: extra pixels in every space between words. Counts the pages again.
- **Letter spacing.** Normal (the default), Slight, Wide or Wider: 0 to 3 extra pixels after every letter, which some readers find easier on small text. Counts the pages again, and is a little slower to draw.
- **Book grid.** Small, Medium or Large tiles for the cover grid of the book list, and a **To read**, **On hold** or **Done** badge on the cover.
- **Reading ruler.** Off, **Line** (the current line is tinted) or **Focus** (every other line is faded). While it is on, up and down move the ruler a line at a time instead of turning pages or scrolling; at the last line of the page the next page starts (in Scroll mode the text scrolls a line). Left and right still move by a page. It is off while you select words.
- **Scroll overlap.** 0 to 3 lines of the old screen stay at the top when left or right scrolls a screen in Scroll mode, so you do not lose your place.
- **Pictures.** Normal, Inverted, High contrast or Hidden. Inverted and High contrast are for dark themes and for line art. Hidden removes pictures from the text.
- **Thin bar.** On draws a 3 pixel line along the bottom edge: the part read is filled and thin ticks mark the chapters.
- **Cover screen.** Off, or 2, 5 or 10 minutes. When no button has been pressed for that long, the cover of the open epub fills the screen; any button brings the book back. Books without a cover picture do not show one.
- **Night theme, Night from, Night until.** Night theme is Off or any of the themes. When it is set, the reader switches to it from the hour in Night from until the hour in Night until (for example 21:00 to 07:00), using the clock of the device, and goes back to your own theme afterwards. A theme you pick by hand during the night is kept.
- **Hyphenate.** On splits a word that does not fit at the end of a line, with a hyphen, so lines are fuller and justified lines have smaller gaps. The patterns for English, French and Portuguese follow the language of the book, and a book in another language uses the English ones. The hyphen is not part of the book, so highlights and bookmarks stay in the same place with Hyphenate on or off. Off is the default. Changing it counts the pages again.
- **Auto exit.** Off, or 5, 10, 15, 30 or 60 minutes. When no button has been pressed for that long, the reader saves its place and quits, so a book left open does not run the battery down.
- **Justify.** On spreads the words of each line so it fills the width, except the last line of a paragraph, headings, centered text, and preformatted text. A line that falls well short of full is left alone. Off is the default.
- **On start.** Library (the default) or Last book: with Last book the reader opens the book you read last as soon as it starts, unless that book is finished.
- **Low battery.** Warn (the default) or Off. When the charge falls to 15% a message says to charge soon; at 5% your place and settings are saved first. Nothing shows while charging.
- **Controls.** Press **A** to open the shortcut guide. While reading it is also in the book menu under **Tools** (**Start** now adds or removes a bookmark).

The margins and line spacing re-wrap the book, so the page count is made again, as it is for the font size.

The four margins, Line spacing, Justify, Indent, Paragraph gap, Bold text, Hyphenate and Reading are kept for each book. Open Settings while a book is open and the change belongs to that book only, and the next time you open it the same layout comes back. Open Settings in the book list and the change sets the defaults every other book starts with. A book you never changed uses the defaults. The layout of a book is in a `.layout` file beside its highlights.

**A** while reading shows or hides the progress line. That choice is saved too.

## What is saved

The reader writes under `App/PixelReader/.pixel_reader_store`.

- Reading position and the last folder you browsed.
- Theme, font, font size, shoulder buttons, whether the progress line is shown, the bar, clock, margins, line spacing, justify, the book list order, and your reading speed.
- A backup file only when you ask for one, in the books folder.
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
