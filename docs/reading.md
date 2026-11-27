# Reading

These are the printed buttons on the Flip.

| Button | While reading |
| --- | --- |
| D-pad up / down | Previous or next line. Hold to keep moving. |
| D-pad left / right | Previous or next page. |
| L1 / R1 | Previous or next page. The other shoulder pair (L2 / R2) jumps to the previous or next chapter: the first press goes to the start of the current chapter, and a second one, or one within the first 3% of a chapter, to the one before. |
| A | Show or hide the progress line. |
| B | Close the book and return to the file list. |
| X | Settings. |
| Y | Select a word near the middle of the page. Commas, periods, and quotes are left outside the selection. |
| Select | The book menu: chapters, search in book, bookmarks, next and previous mark, book map, skim the book, go to a percent, go to a page, view pictures, jump back, look up a word, reading stats, word list, highlights. Press Select again to close it. |
| Start | Add a bookmark on this page, or remove the one that is there. |
| Menu | Quit the app. A tap of Menu quits. A Menu chord, such as the brightness shortcut, does not. |

The progress line shows the chapter name and how far you are.

**Scroll** is the normal way to read. Up and down move a line, and left and right move a screen. The progress line shows the percentage, because a page number does not belong to a continuous scroll. The percentage follows the text and does not jump when you turn a screen.

**Page** counts real screens from the start of the book or chapter. The progress line shows `12 / 248` plus the percent. When you switch to Page, the reader finishes that count and then jumps to the top of the page you were on, so you are never left in the middle of a page. After that, the D-pad and the shoulder buttons turn a whole page. Up and down do not scroll a line. Each turn adds 1. A new chapter, a title page, and a picture that does not fit the rest of the page start at the top of the next page. The blank line between two paragraphs never opens a page, so a page always starts on text. While the pages are counted, a **Counting pages** screen with a progress bar replaces the text, so nothing is drawn half done and no button turns a page that is not counted yet. **B** still leaves the book. When the count finishes, the book appears at the top of the page you were on. Changing the font counts again and lines you up the same way.

**Book** or **Chapter** applies to both. Book is the whole file. Chapter is the current chapter, so the percentage and the page count both start over at each chapter. In Chapter mode a page turn stops at the edge of the chapter. Turning past the last page of a chapter moves to the first line after it, and turning back from the first page goes to the last page of the previous chapter, so you are never stuck inside one chapter. The new chapter is counted before the page is drawn, so the screen shows the finished page and does not jump.

## Word lookup and highlights

Press **Y** on a page. A cursor sits on a word near the middle of the page (in Page mode, the middle of that page), and the bar names the buttons. A comma, period, quote, or similar mark attached to that word is not part of the selection.

| Button | While words are selected |
| --- | --- |
| D-pad | Move the cursor. Hold to repeat. Left at the start of the page, or right at the end, turns the page. Up at the first line or down at the last line does too in Page mode, and the cursor lands on the first (or last) line of the new page; in Scroll mode the text moves a line. Blank lines between paragraphs are passed over. Only the words of the page on screen can be reached in Page mode. Until you extend, each move is still one word. |
| Start | Keep the first word and add every word you move through. Press Start again to stop extending. The next move returns to one word. |
| X | Grow the selection: first to the whole sentence (up to a full stop, question or exclamation mark), then to the whole paragraph, then back to the word you started from. It works on the words on the page. Then **A** can look the phrase up or highlight all of it. |
| A | Open a small menu over the page, with **Look up**, **Look up in...** (when there is more than one dictionary), **Search book**, **Highlight**, **Underline**, and **Strike out**. Look up and Search use the word at the cursor. The three marks cover the whole selection. If that stretch is already marked, the last rows are replaced by **Remove highlight**. **B** returns to the word. |
| B or Y | Leave word selection. The shoulders still turn the page. |

**Look up** opens the definition full screen. Above the entry a line says how often the word turns up in this book, such as `In this book: 14 times`, with `(rare)` for one to three times; a phrase has no line, and **Style tweaks > No word count in lookups** turns it off for books where counting is slow. Up and down scroll a long definition. Any other button closes it.

The dictionary follows the language the book declares (`dc:language`): `en`, `fr`, `pt`, `es`, `it`, `de` or `ru`, with any region such as `pt-BR`. A book in another language, a `.txt` file, or a language whose dictionary file is not on the card uses the English one.

- **English** is WordNet 3.1, one gloss per word, the first sense that was kept.
- **French**, **Portuguese**, **Spanish**, **Italian**, **German** and **Russian** come from the English Wiktionary through [Kaikki](https://kaikki.org). They give an English gloss per word, up to three part-of-speech readings, and they include inflected forms, such as `plural of livre`, `first-person plural present indicative of hablar` or, in Russian, `inflection of книга: genitive singular`. None of them knows phrases. In Russian the stress marks are left out and capital letters match their lowercase form. Spanish, Italian, German and Russian have an entry for each inflected form, so a word is found as typed; they have no hyphenation patterns, so a book in one of them is not hyphenated.

Lookup tries the word, then the word without `'s`, then the word without a trailing `s`, then the part after an apostrophe (`l'homme` becomes `homme`). Accented letters are kept, and a capital accented letter matches its lowercase form. If the dictionary file is missing, Look up says there is no entry. A dictionary is read from the card the first time you look something up in it, and a **Loading dictionary** screen with a bar shows that, so the screen never just freezes.

The first time a book is opened, the reader has to measure every part of it. An **Opening book** screen with a bar shows that. The result is remembered, so the next opening is quick.

On a word that is a link in the book, such as a footnote number, the **A** menu also has **Show note**, which opens the text the link points to when it names a spot in the book, and **Go to link**, which jumps there. **Jump back** in the book menu returns.

**Search book** finds the word everywhere in the book and lists the places with the words around each one, their place as a percent and the chapter. **Search in book** in the book menu (**Select**) takes any word or phrase and keeps your last eight searches; it has four switches, **Whole words** (on by default), **Match case** (off by default), **Match accents** (off by default, so a search for `cafe` also finds `café`) and **This chapter only** (off by default), kept between sessions. The word menu's **Search book** uses the same switches. **A** goes there and **Jump back** returns. The list has one row for each paragraph that holds the word, up to 300 rows. The search reads the whole book, so a **Searching** screen with a bar shows how far it has got. If the word is split between two lines with a hyphen, Look up and Search use the whole word.

A word that has no entry as typed is tried without its ending, in the rules of the dictionary being used (English, French or Portuguese; the other dictionaries list the inflected forms themselves): `running` shows `run`, `chevaux` shows `cheval`, `falamos` shows `falar`, `l'homme` shows `homme`, and the entry begins with the word that was found in brackets. **Words of this book** in the book menu (Words) lists only the words looked up in this book, each with the sentence it was in; **A** shows the sentence under the meaning there and in the full word list. Every word you look up is added to the **Word list** (and can be practised with **Review words**, see the book menu below), newest first, with its meaning and the book it came from. Looking a word up again moves it to the top. **A** shows the meaning, **X** deletes a word. **Export word list** in the book menu writes them to `Word list.txt` in the folder of the book. The list is kept in `vocabulary.tsv` in the store and is shared by all books.

New highlights use the color chosen in Settings (**Highlight color**: Yellow, Green, Blue or Pink), and each highlight keeps its own color, so you can change the setting and go on with another color. Colors are tuned for light and dark themes. Exports mark colored ones, such as `[Green]`.

**Highlight** marks the selected words and saves them with the book, along with the words themselves. A highlight is a bar behind each word. **Underline** draws a line under the words instead, and **Strike out** draws one through them. Each mark remembers its own style. Punctuation between the words is not painted. It is still there after you close the book. Removing it deletes the marks that overlap the selection. Installing the reader again does not delete highlights.

Each highlight remembers the day it was made (shown in the list after its place); **Highlight tools > List highlights newest first** sorts the list that way, until you close the book. **Undo delete** in the book menu (Marks) brings back the last highlights, bookmarks or words you deleted (one deletion at a time, up to 20, until you close the book); a note you cleared or changed comes back too. **Highlight tools** in the book menu (Marks) exports or deletes all the highlights of the chapter you are in, and for each kind of highlight in use (a color, underlines, strikeouts) changes them all to another kind or deletes them all. The chapter export goes in a file of its own, `<title> - highlights - <chapter>.md`. The **Highlights** row of the book menu lists every highlight in book order with its place as a percent. **A** goes there, **Y** writes a **note** on the highlight (an empty note removes it; a highlight with a note has a `*` in front), **X** removes one and **Start** narrows the list to one kind: with notes, each marker color, underlines or strikeouts (only kinds that exist are offered; the title says how many of how many). Highlights keep no date, so they cannot be ordered by it. Notes are kept with the highlights and also show in the library list **All highlights** in square brackets, where the search finds them. **Export highlights and notes** writes `<title> - highlights.md` in the folder of the book: a Markdown file with the chapter names, each highlight as a quotation, its note, its place and color. A hidden comment after each one holds its exact place, so **Import highlights** (shown in the book menu when that file exists) reads the file back into the book, on this card or another copy of the same book; highlights already there are kept and missing notes are filled in. The format is the reader's own, not KOReader's. **Library tools > Export all highlights and notes** writes `All highlights.md` in the books folder, with every book's highlights and notes. A highlight made before this version has no saved words and shows as `(text not saved)`.

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
