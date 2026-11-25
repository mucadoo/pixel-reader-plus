#ifndef BOOK_SEARCH_H_
#define BOOK_SEARCH_H_

#include "doc_api/doc_addr.h"

#include <functional>
#include <string>
#include <vector>

class DocReader;

struct SearchHit
{
    DocAddr address = 0;
    // The words around the match.
    std::string snippet;
};

// Every place the word turns up in the book, in order, ignoring case and
// matching whole words only. Stops after max_hits. progress is told how far
// through the book the search is, 0 to 100.
std::vector<SearchHit> search_book(
    const DocReader &reader,
    const std::string &word,
    size_t max_hits,
    const std::function<void(int percent)> &progress
);

#endif
