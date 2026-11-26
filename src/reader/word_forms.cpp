#include "./word_forms.h"

#include <algorithm>
#include <map>

namespace word_forms
{

namespace
{

bool ends_with(const std::string &text, const std::string &suffix)
{
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The number of letters in a UTF-8 string.
size_t letters(const std::string &text)
{
    size_t count = 0;
    for (unsigned char c : text)
    {
        if ((c & 0xC0) != 0x80)
        {
            ++count;
        }
    }
    return count;
}

struct Collector
{
    const std::string &word;
    std::vector<std::string> out;

    void add(const std::string &candidate)
    {
        if (letters(candidate) < 2 || candidate == word)
        {
            return;
        }
        if (std::find(out.begin(), out.end(), candidate) == out.end())
        {
            out.push_back(candidate);
        }
    }

    // word with `cut` bytes off the end and `put` added
    void swap_ending(const std::string &suffix, const std::string &put)
    {
        if (ends_with(word, suffix) && word.size() > suffix.size())
        {
            add(word.substr(0, word.size() - suffix.size()) + put);
        }
    }
};

// A stem with a doubled last letter, "runn", loses one.
std::string undouble(const std::string &stem)
{
    if (stem.size() >= 3 && stem[stem.size() - 1] == stem[stem.size() - 2] && stem.back() != 's' && stem.back() != 'l')
    {
        return stem.substr(0, stem.size() - 1);
    }
    return stem;
}

const std::map<std::string, std::string> &english_irregular()
{
    static const std::map<std::string, std::string> forms = {
        {"was", "be"}, {"were", "be"}, {"been", "be"}, {"is", "be"}, {"am", "be"}, {"are", "be"}, {"has", "have"}, {"had", "have"},
        {"does", "do"}, {"did", "do"}, {"done", "do"}, {"went", "go"}, {"gone", "go"}, {"goes", "go"}, {"saw", "see"}, {"seen", "see"},
        {"took", "take"}, {"taken", "take"}, {"came", "come"}, {"made", "make"}, {"said", "say"}, {"got", "get"}, {"gotten", "get"},
        {"gave", "give"}, {"given", "give"}, {"knew", "know"}, {"known", "know"}, {"thought", "think"}, {"found", "find"},
        {"told", "tell"}, {"felt", "feel"}, {"left", "leave"}, {"kept", "keep"}, {"began", "begin"}, {"begun", "begin"},
        {"brought", "bring"}, {"bought", "buy"}, {"caught", "catch"}, {"taught", "teach"}, {"fought", "fight"}, {"sought", "seek"},
        {"wrote", "write"}, {"written", "write"}, {"rode", "ride"}, {"ridden", "ride"}, {"drove", "drive"}, {"driven", "drive"},
        {"spoke", "speak"}, {"spoken", "speak"}, {"broke", "break"}, {"broken", "break"}, {"chose", "choose"}, {"chosen", "choose"},
        {"fell", "fall"}, {"fallen", "fall"}, {"ate", "eat"}, {"eaten", "eat"}, {"sat", "sit"}, {"stood", "stand"}, {"ran", "run"},
        {"held", "hold"}, {"heard", "hear"}, {"met", "meet"}, {"paid", "pay"}, {"sent", "send"}, {"spent", "spend"}, {"built", "build"},
        {"lost", "lose"}, {"won", "win"}, {"slept", "sleep"}, {"woke", "wake"}, {"woken", "wake"}, {"threw", "throw"}, {"thrown", "throw"},
        {"grew", "grow"}, {"grown", "grow"}, {"flew", "fly"}, {"flown", "fly"}, {"sang", "sing"}, {"sung", "sing"}, {"swam", "swim"},
        {"hid", "hide"}, {"hidden", "hide"}, {"bit", "bite"}, {"bitten", "bite"}, {"shook", "shake"}, {"shaken", "shake"},
        {"children", "child"}, {"men", "man"}, {"women", "woman"}, {"feet", "foot"}, {"teeth", "tooth"}, {"mice", "mouse"},
        {"geese", "goose"}, {"people", "person"}, {"lives", "life"}, {"wives", "wife"}, {"knives", "knife"}, {"wolves", "wolf"},
        {"leaves", "leaf"}, {"better", "good"}, {"best", "good"}, {"worse", "bad"}, {"worst", "bad"}, {"his", "he"}, {"him", "he"},
        {"her", "she"}, {"them", "they"}, {"their", "they"}, {"us", "we"}, {"our", "we"}, {"me", "i"}, {"my", "i"}
    };
    return forms;
}

void english(Collector &c)
{
    const std::string &w = c.word;
    // Contractions and possessives.
    for (const char *tail : {"n't", "'ll", "'re", "'ve", "'d", "'m", "'s", "s'"})
    {
        c.swap_ending(tail, tail == std::string("s'") ? "s" : "");
    }
    for (const char *tail : {"\xE2\x80\x99s", "\xE2\x80\x99ll", "\xE2\x80\x99re", "\xE2\x80\x99ve", "\xE2\x80\x99" "d", "\xE2\x80\x99m", "n\xE2\x80\x99t"})
    {
        c.swap_ending(tail, "");
    }
    const auto found = english_irregular().find(w);
    if (found != english_irregular().end())
    {
        c.add(found->second);
    }
    if (ends_with(w, "ies") && w.size() > 4)
    {
        c.swap_ending("ies", "y");
    }
    if (ends_with(w, "ied") && w.size() > 4)
    {
        c.swap_ending("ied", "y");
    }
    if (ends_with(w, "ier") || ends_with(w, "iest"))
    {
        c.swap_ending("ier", "y");
        c.swap_ending("iest", "y");
    }
    if (ends_with(w, "ily"))
    {
        c.swap_ending("ily", "y");
    }
    if (ends_with(w, "ves"))
    {
        c.swap_ending("ves", "f");
        c.swap_ending("ves", "fe");
    }
    for (const char *suffix : {"ing", "ed", "er", "est"})
    {
        if (ends_with(w, suffix) && w.size() > std::string(suffix).size() + 1)
        {
            const std::string stem = w.substr(0, w.size() - std::string(suffix).size());
            c.add(undouble(stem));
            c.add(stem);
            c.add(stem + "e");
            if (ends_with(w, "ing") && ends_with(stem, "y"))
            {
                c.add(stem.substr(0, stem.size() - 1) + "ie"); // dying, lying
            }
        }
    }
    if (ends_with(w, "ed") || ends_with(w, "es"))
    {
        c.swap_ending(ends_with(w, "ed") ? "d" : "s", "");
    }
    if (ends_with(w, "es"))
    {
        c.swap_ending("es", "");
    }
    if (ends_with(w, "s") && !ends_with(w, "ss") && !ends_with(w, "us"))
    {
        c.swap_ending("s", "");
    }
    if (ends_with(w, "ly"))
    {
        c.swap_ending("ly", "");
        c.swap_ending("ally", "al");
    }
    if (ends_with(w, "men"))
    {
        c.swap_ending("men", "man");
    }
}

struct Rule
{
    const char *suffix;
    const char *put;
};

void with_rules(Collector &c, const std::vector<Rule> &rules)
{
    for (const Rule &rule : rules)
    {
        c.swap_ending(rule.suffix, rule.put);
    }
}

void french(Collector &c)
{
    std::string w = c.word;
    // l'homme, qu'il, aujourd'hui: the part after the apostrophe is the word.
    for (const char *mark : {"'", "\xE2\x80\x99"})
    {
        const size_t at = w.rfind(mark);
        if (at != std::string::npos && at + std::string(mark).size() < w.size())
        {
            c.add(w.substr(at + std::string(mark).size()));
            w = w.substr(at + std::string(mark).size());
        }
    }
    Collector inner{w, {}};
    // Nouns and adjectives: plural, then feminine.
    with_rules(inner, {
        {"eaux", "eau"}, {"aux", "al"}, {"aux", "ail"}, {"euses", "eur"}, {"euse", "eur"}, {"euses", "eux"}, {"euse", "eux"},
        {"trices", "teur"}, {"trice", "teur"}, {"ives", "if"}, {"ive", "if"}, {"\xC3\xA8res", "er"}, {"\xC3\xA8re", "er"},
        {"elles", "el"}, {"elle", "el"}, {"ennes", "en"}, {"enne", "en"}, {"ettes", "et"}, {"ette", "et"},
        {"onnes", "on"}, {"onne", "on"}, {"\xC3\xA9""es", "\xC3\xA9"}, {"\xC3\xA9""e", "\xC3\xA9"}, {"ales", "al"}, {"ale", "al"},
        {"s", ""}, {"x", ""}, {"es", ""}, {"e", ""}
    });
    // Verbs: take the ending off and put an infinitive ending on.
    static const char *endings[] = {
        "aient", "erions", "eriez", "eraient", "eront", "erais", "erait", "erai", "eras", "era", "erons", "erez", "ions", "iez",
        "issions", "issiez", "issaient", "issons", "issez", "issent", "issant", "issais", "issait", "ais", "ait", "ant", "ons", "ez",
        "ent", "es", "e", "\xC3\xA9""es", "\xC3\xA9""e", "\xC3\xA9s", "\xC3\xA9", "ai", "as", "a", "\xC3\xA8rent", "\xC3\xA2mes", "\xC3\xA2tes",
        "is", "it", "i", "ies", "ie", "us", "ue", "ues", "u", "irai", "iras", "ira", "irons", "irez", "iront", "irais", "irait", "ent"
    };
    for (const char *ending : endings)
    {
        if (ends_with(w, ending) && w.size() > std::string(ending).size() + 1)
        {
            const std::string stem = w.substr(0, w.size() - std::string(ending).size());
            const bool er_first = std::string(ending).rfind("\xC3\xA9", 0) == 0 || std::string(ending) == "ant" || std::string(ending) == "ez" || std::string(ending) == "ons" || std::string(ending) == "e" || std::string(ending) == "es" || std::string(ending) == "ent";
            if (er_first)
            {
                inner.add(stem + "er");
                inner.add(stem + "ir");
                inner.add(stem + "re");
            }
            else
            {
                inner.add(stem + "ir");
                inner.add(stem + "re");
                inner.add(stem + "er");
            }
            // "mangeons" -> "manger" (the e only keeps the g soft), "commen\xC3\xA7ons" -> "commencer".
            if (ends_with(stem, "ge"))
            {
                inner.add(stem.substr(0, stem.size() - 1) + "er");
            }
            if (ends_with(stem, "\xC3\xA7"))
            {
                inner.add(stem.substr(0, stem.size() - 2) + "cer");
            }
        }
    }
    for (const std::string &candidate : inner.out)
    {
        c.add(candidate);
    }
}

void portuguese(Collector &c)
{
    const std::string &w = c.word;
    with_rules(c, {
        {"\xC3\xB5""es", "\xC3\xA3o"}, {"\xC3\xA3""es", "\xC3\xA3o"}, {"\xC3\xA3os", "\xC3\xA3o"}, {"ais", "al"}, {"eis", "el"},
        {"\xC3\xB3is", "ol"}, {"uis", "ul"}, {"ns", "m"}, {"res", "r"}, {"zes", "z"}, {"eiras", "eiro"}, {"eira", "eiro"},
        {"oras", "or"}, {"ora", "or"}, {"esa", "\xC3\xAA" "s"}, {"onas", "\xC3\xA3o"}, {"ona", "\xC3\xA3o"}, {"as", "o"}, {"a", "o"},
        {"s", ""}
    });
    if (ends_with(w, "mente"))
    {
        c.swap_ending("mente", "");
        if (ends_with(w, "amente"))
        {
            c.swap_ending("amente", "o");
        }
    }
    // Verbs: the endings that go with ar, er or ir.
    struct Ending
    {
        const char *text;
        int first; // 0 ar, 1 er, 2 ir
    };
    static const Ending endings[] = {
        {"ando", 0}, {"endo", 1}, {"indo", 2}, {"ado", 0}, {"ada", 0}, {"ados", 0}, {"adas", 0}, {"ido", 1}, {"ida", 1}, {"idos", 1},
        {"idas", 1}, {"amos", 0}, {"emos", 1}, {"imos", 2}, {"aram", 0}, {"eram", 1}, {"iram", 2}, {"avam", 0}, {"ava", 0}, {"avas", 0},
        {"iam", 1}, {"ia", 1}, {"ias", 1}, {"arei", 0}, {"erei", 1}, {"irei", 2}, {"ar\xC3\xA1", 0}, {"er\xC3\xA1", 1}, {"ir\xC3\xA1", 2},
        {"aria", 0}, {"eria", 1}, {"iria", 2}, {"am", 0}, {"em", 1}, {"ou", 0}, {"eu", 1}, {"iu", 2}, {"ei", 0}, {"o", 0}, {"as", 0},
        {"es", 1}, {"e", 1}, {"a", 0}, {"\xC3\xA1", 0}, {"\xC3\xA9", 1}, {"emos", 1}
    };
    static const char *infinitives[] = {"ar", "er", "ir"};
    for (const Ending &ending : endings)
    {
        if (ends_with(w, ending.text) && w.size() > std::string(ending.text).size() + 1)
        {
            const std::string stem = w.substr(0, w.size() - std::string(ending.text).size());
            c.add(stem + infinitives[ending.first]);
            for (int i = 0; i < 3; ++i)
            {
                if (i != ending.first)
                {
                    c.add(stem + infinitives[i]);
                }
            }
        }
    }
}

} // namespace

Language language_for(const std::string &dictionary_name)
{
    std::string name = dictionary_name;
    for (char &c : name)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    if (name.rfind("fr", 0) == 0 || name.find("french") != std::string::npos || name.find("fran") != std::string::npos)
    {
        return Language::French;
    }
    if (name.rfind("pt", 0) == 0 || name.find("portug") != std::string::npos)
    {
        return Language::Portuguese;
    }
    return Language::English;
}

std::vector<std::string> base_forms(const std::string &word, Language language)
{
    Collector collector{word, {}};
    switch (language)
    {
        case Language::French: french(collector); break;
        case Language::Portuguese: portuguese(collector); break;
        default: english(collector); break;
    }
    return collector.out;
}

} // namespace word_forms
