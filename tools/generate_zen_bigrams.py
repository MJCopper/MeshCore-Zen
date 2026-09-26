#!/usr/bin/env python3
"""Generate Zen's compact bigram and trigram prediction tables.

The source is the official tab-separated bigram CSV archive. Only pairs whose
words exist in Zen's conversational dictionary are retained. The generated
firmware table stores word IDs, never duplicate strings or source frequencies.
"""

import argparse
import csv
from collections import Counter, defaultdict
import re
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


PREDECESSOR_COUNT = 2000
SUCCESSOR_COUNT = 8
TRIGRAM_CONTEXT_COUNT = 300
TRIGRAM_SUCCESSOR_COUNT = 3
SENTENCE_START_COUNT = 12
SPECIAL_WORDS = ("a", "i")

AUSTRALIAN_SPELLINGS = {
    "apologize": "apologise", "behavior": "behaviour", "center": "centre",
    "color": "colour", "defense": "defence", "favor": "favour",
    "favorite": "favourite", "gray": "grey", "honor": "honour",
    "labor": "labour", "license": "licence", "neighbor": "neighbour",
    "organize": "organise", "realize": "realise", "theater": "theatre",
    "tire": "tyre", "traveled": "travelled", "traveling": "travelling",
}

# Small chat-specific corrections take precedence over subtitle frequencies.
OVERRIDES = {
    "are": ("you", "we", "they", "there"),
    "do": ("you", "not", "we", "it"),
    "good": ("morning", "night", "luck", "idea"),
    "how": ("are", "is", "do", "was"),
    "i": ("am", "have", "think", "will"),
    "see": ("you", "what", "how", "the"),
    "thank": ("you",),
}

TRIGRAM_OVERRIDES = {
    ("are", "you"): ("okay", "going", "coming"),
    ("can", "you"): ("please", "help", "see"),
    ("do", "you"): ("know", "want", "have"),
    ("good", "morning"): ("how", "everyone"),
    ("good", "night"): ("see", "you"),
    ("have", "a"): ("good", "great", "nice"),
    ("how", "are"): ("you", "things"),
    ("i", "am"): ("going", "not", "here"),
    ("i", "will"): ("be", "see", "call"),
    ("let", "me"): ("know", "see", "check"),
    ("on", "my"): ("way", "own"),
    ("see", "you"): ("soon", "later", "there"),
    ("thank", "you"): ("for", "very"),
    ("where", "are"): ("you", "we"),
}

SENTENCE_START = (
    "i", "the", "we", "you", "hi", "thanks", "can", "are", "good",
    "please", "hey", "what",
)


def dictionary_words(core: Path, extra: Path) -> list[str]:
    core_text = core.read_text(encoding="utf-8")
    core_body = core_text.split("static const char* const WORDS[BASE_WORD_COUNT] = {", 1)[1].split("};", 1)[0]
    extra_text = extra.read_text(encoding="utf-8")
    extra_body = extra_text.split("static const char* const EXTRA_WORDS[EXTRA_WORD_COUNT] = {", 1)[1].split("};", 1)[0]
    words = re.findall(r'"([a-z\']+)"', core_body + extra_body)
    if len(words) != 4000 or len(set(words)) != 4000:
        raise RuntimeError(f"expected 4,000 unique dictionary words, found {len(words)}")
    return words


def normalise(word: str) -> str:
    word = word.strip().lower()
    return AUSTRALIAN_SPELLINGS.get(word, word)


def generate_bigrams(source: Path, words: list[str]) -> dict[str, list[str]]:
    allowed = set(words) | set(SPECIAL_WORDS)
    predecessors = (set(words[:PREDECESSOR_COUNT - len(SPECIAL_WORDS)]) |
                    set(SPECIAL_WORDS))
    successors = {word: list(OVERRIDES.get(word, ())) for word in predecessors}

    with zipfile.ZipFile(source) as archive:
        names = archive.namelist()
        if len(names) != 1:
            raise RuntimeError("expected one CSV in the SUBTLEX-UK archive")
        with archive.open(names[0]) as raw:
            lines = (line.decode("utf-8", errors="replace") for line in raw)
            reader = csv.DictReader(lines, delimiter="\t")
            for row in reader:
                previous = normalise(row["spelling"])
                following = normalise(row["spelling1"])
                if previous not in predecessors or following not in allowed:
                    continue
                choices = successors[previous]
                if following != previous and following not in choices and len(choices) < SUCCESSOR_COUNT:
                    choices.append(following)

    return {word: choices for word, choices in successors.items() if choices}


def generate_trigrams(source: Path, words: list[str]) -> dict[tuple[str, str], list[str]]:
    allowed = set(words) | set(SPECIAL_WORDS)
    predecessors = (set(words[:PREDECESSOR_COUNT - len(SPECIAL_WORDS)]) |
                    set(SPECIAL_WORDS))
    counts: dict[tuple[str, str], Counter[str]] = defaultdict(Counter)
    with zipfile.ZipFile(source) as archive:
        names = archive.namelist()
        if len(names) != 1:
            raise RuntimeError("expected one XML in the NUS SMS archive")
        with archive.open(names[0]) as raw:
            for _, element in ET.iterparse(raw, events=("end",)):
                if element.tag != "text":
                    continue
                for sentence in re.split(r"[.!?]+", element.text or ""):
                    tokens = [normalise(token) for token in
                              re.findall(r"[A-Za-z]+(?:'[A-Za-z]+)?", sentence)]
                    for first, second, following in zip(tokens, tokens[1:], tokens[2:]):
                        if (first in predecessors and second in predecessors and
                                following in allowed):
                            counts[(first, second)][following] += 1
                element.clear()

    ranked = sorted(counts, key=lambda pair: (-sum(counts[pair].values()), pair))
    selected: dict[tuple[str, str], list[str]] = {
        pair: list(successors) for pair, successors in TRIGRAM_OVERRIDES.items()
    }
    for pair in ranked:
        if pair in selected:
            choices = selected[pair]
        elif len(selected) < TRIGRAM_CONTEXT_COUNT:
            choices = selected.setdefault(pair, [])
        else:
            continue
        for word, frequency in counts[pair].most_common():
            if frequency < 2:
                break
            if word not in choices and word not in pair:
                choices.append(word)
            if len(choices) == TRIGRAM_SUCCESSOR_COUNT:
                break
        if len(selected) >= TRIGRAM_CONTEXT_COUNT and pair not in TRIGRAM_OVERRIDES:
            continue
    selected = {pair: choices[:TRIGRAM_SUCCESSOR_COUNT]
                for pair, choices in selected.items() if choices}
    if len(selected) != TRIGRAM_CONTEXT_COUNT:
        raise RuntimeError(f"expected {TRIGRAM_CONTEXT_COUNT} trigram contexts, found {len(selected)}")
    return selected


def render(pairs: dict[str, list[str]], trigrams: dict[tuple[str, str], list[str]],
           words: list[str]) -> str:
    word_ids = {word: index for index, word in enumerate(words)}
    word_ids["a"] = len(words)
    word_ids["i"] = len(words) + 1
    ordered = sorted(pairs.items())
    lines = [
        "#pragma once", "", "#include <stdint.h>", "", "namespace zen_context_data {", "",
        "// Generated by tools/generate_zen_bigrams.py from SUBTLEX-UK bigram",
        "// frequencies (van Heuven et al., 2014). Do not edit by hand.",
        f"static const uint16_t WORD_A = {word_ids['a']};",
        f"static const uint16_t WORD_I = {word_ids['i']};",
        "static const uint16_t NO_WORD = 0xFFFF;", "",
        f"static const uint8_t SUCCESSOR_COUNT = {SUCCESSOR_COUNT};",
        "struct Entry {", "  uint16_t previous;", f"  uint16_t next[{SUCCESSOR_COUNT}];", "};", "",
        f"static const Entry ENTRIES[{len(ordered)}] = {{",
    ]
    for previous, following in ordered:
        ids = [word_ids[word] for word in following]
        ids.extend([0xFFFF] * (SUCCESSOR_COUNT - len(ids)))
        values = ", ".join("NO_WORD" if value == 0xFFFF else str(value) for value in ids)
        lines.append(f"  {{ {word_ids[previous]}, {{ {values} }} }}, // {previous}")
    lines.extend(["};", f"static const uint16_t ENTRY_COUNT = {len(ordered)};", "",
                  f"static const uint8_t TRIGRAM_SUCCESSOR_COUNT = {TRIGRAM_SUCCESSOR_COUNT};",
                  "struct TrigramEntry {", "  uint16_t first;", "  uint16_t second;",
                  f"  uint16_t next[{TRIGRAM_SUCCESSOR_COUNT}];", "};", "",
                  f"static const TrigramEntry TRIGRAMS[{len(trigrams)}] = {{"])
    for (first, second), following in sorted(
            trigrams.items(), key=lambda item: (word_ids[item[0][0]], word_ids[item[0][1]])):
        ids = [word_ids[word] for word in following]
        ids.extend([0xFFFF] * (TRIGRAM_SUCCESSOR_COUNT - len(ids)))
        values = ", ".join("NO_WORD" if value == 0xFFFF else str(value) for value in ids)
        lines.append(f"  {{ {word_ids[first]}, {word_ids[second]}, {{ {values} }} }}, // {first} {second}")
    lines.extend(["};", f"static const uint16_t TRIGRAM_COUNT = {len(trigrams)};", "",
                  f"static const uint16_t SENTENCE_START[{SENTENCE_START_COUNT}] = {{",
                  "  " + ", ".join(str(word_ids[word]) for word in SENTENCE_START) + ",",
                  "};", "", "} // namespace zen_context_data", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="SUBTLEX-UK_bigrams.csv.zip")
    parser.add_argument("output", type=Path)
    parser.add_argument("--trigram-source", type=Path, required=True,
                        help="NUS English SMS XML zip archive")
    parser.add_argument("--core", type=Path,
                        default=Path("examples/companion_radio/zen-overlay/app/zen/WordCompleter.h"))
    parser.add_argument("--extra", type=Path,
                        default=Path("examples/companion_radio/zen-overlay/app/zen/ZenWordDictionaryExtra.h"))
    args = parser.parse_args()
    words = dictionary_words(args.core, args.extra)
    args.output.write_text(render(generate_bigrams(args.source, words),
                                  generate_trigrams(args.trigram_source, words), words),
                           encoding="utf-8")


if __name__ == "__main__":
    main()
