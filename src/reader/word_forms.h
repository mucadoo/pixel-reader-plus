#ifndef WORD_FORMS_H_
#define WORD_FORMS_H_

#include <string>
#include <vector>

// Guesses at the base form of an inflected word, for a lookup that has no entry as typed:
// "running" -> "run", "livres" -> "livre", "falamos" -> "falar".
namespace word_forms
{

// Which rules to use, from the name of a dictionary such as "english", "french" or "portuguese-big".
enum class Language
{
    English,
    French,
    Portuguese,
    Other // a dictionary with its own entries for inflected forms, so no endings are cut
};
Language language_for(const std::string &dictionary_name);

// The words to try, most likely first, never the word itself and none shorter than two letters.
// The word is expected in lowercase. A word with no apostrophe-cut or ending to take off gives none.
std::vector<std::string> base_forms(const std::string &word, Language language);

} // namespace word_forms

#endif
