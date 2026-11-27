# Builds a dictionary file for the reader from a Kaikki extract of the English Wiktionary.
#   python3 kaikki_dictionary.py kaikki.org-dictionary-Spanish.jsonl spanish.txt Spanish
# The extract is downloaded from https://kaikki.org/dictionary/<Language>/. The output is one
# word<TAB>gloss line for each word, sorted by the bytes of the word, with up to three readings.
import json, sys, re

src, dst, label = sys.argv[1], sys.argv[2], sys.argv[3]
POS = {"noun":"n.","verb":"v.","adj":"adj.","adv":"adv.","name":"name","prep":"prep.","pron":"pron.",
       "conj":"conj.","intj":"interj.","det":"det.","num":"num.","article":"art.","particle":"part.",
       "contraction":"contr.","prefix":"pref.","suffix":"suff.","phrase":"phrase","proverb":"prov."}
entries = {}
count = 0
with open(src, "r", encoding="utf-8") as fh:
    for line in fh:
        try:
            obj = json.loads(line)
        except ValueError:
            continue
        if obj.get("lang") and obj["lang"] != label:
            continue
        word = obj.get("word", "")
        key = word.replace("\u0301", "").lower().strip()
        if not key or " " in key or "\t" in key or "\n" in key or len(key) > 40:
            continue
        pos = POS.get(obj.get("pos", ""), obj.get("pos", ""))
        for sense in obj.get("senses", []):
            glosses = sense.get("glosses")
            if not glosses:
                continue
            gloss = glosses[0]
            if gloss.endswith(":") and len(glosses) > 1:
                gloss = gloss + " " + glosses[1]
            gloss = re.sub(r"\s+", " ", gloss.replace("\u0301", "")).strip()
            if not gloss:
                continue
            bucket = entries.setdefault(key, [])
            text = "(%s) %s" % (pos, gloss) if pos else gloss
            if text not in bucket and len(bucket) < 3:
                bucket.append(text)
            break  # first sense of each entry
        count += 1

rows = []
for key, glosses in entries.items():
    gloss = "; ".join(glosses)
    if len(gloss) > 320:
        gloss = gloss[:317].rsplit(" ", 1)[0] + "..."
    rows.append((key.encode("utf-8"), gloss.encode("utf-8")))
rows.sort(key=lambda r: r[0])
with open(dst, "wb") as out:
    for key, gloss in rows:
        out.write(key + b"\t" + gloss + b"\n")
print(label, "entries read", count, "words", len(rows))