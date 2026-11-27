# What is saved

The reader writes under `App/PixelReader/.pixel_reader_store`.

- Reading position and the last folder you browsed.
- Theme, font, font size, whether the progress line is shown, the bar, clock, margins, line spacing, justify, and the book list order.
- A backup file only when you ask for one, in the books folder.
- Highlights, bookmarks and the layout, one file for each of them per book, named from the book file. A highlight line also holds the style, the time it was made (`@` and the seconds since 1970), its words and its note.
- `stats.log` and `vocabulary.tsv`: the reading time for each day and book, and the words you looked up.
- `pending_progress.tsv` and `group_series`: places set by hand, and whether series are grouped.
- `collections.tsv` and `smart_collections.tsv`: the books of each collection, and the conditions of each smart collection.
- `hidden_folders.txt`: the folders hidden from the library, one path a line.
- `folders.tsv` and `profiles.tsv`: the view chosen for each folder, and the saved layout profiles.
- `opening.txt`: exists only while a book is opening and shortly after; if it is found at the next start the reader stopped, and asks what to do (see After a crash). It is not part of a backup you need to keep.
- `dictionaries.txt`, `vocabulary_review.tsv` and `vocabulary_context.tsv`: the dictionary order, the flash card schedule and the sentences kept with looked-up words.
- `library.log`: for each book you opened, how far you got and when, whether it is a favorite or marked finished, its rating, and the name of its saved data. The list uses it for progress, sorting by recent, Continue, favorites and finished.
- `info`, `covers`, `fonts` and the `.cache` files in `books`: the titles, the cover pictures, the fonts copied out of books and the saved page layouts. They can be deleted at any time and are made again; **Library tools > Storage** does it for you and shows their size.

Books themselves are not copied or modified. A later install of this reader replaces the program and the dictionary, and leaves the store alone.
