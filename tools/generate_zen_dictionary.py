#!/usr/bin/env python3
"""Generate Zen's supplementary conversational word dictionary.

The input is the ISC-licensed ``index.json`` from the
``subtlex-word-frequencies`` package.  The original 2,500-word dictionary is
kept as the curated, stable core; this script appends lower-frequency words
without changing existing completion rankings.
"""

import argparse
import json
import re
from pathlib import Path


TARGET_EXTRA_WORDS = 1500
START_AFTER = "bail"
WORD_RE = re.compile(r"[a-z]{3,15}")

# Project-specific words retained at the end of the fixed-size dictionary so
# adding them cannot renumber the frequency-ranked words used by context data.
PINNED_WORDS = ("hillvue",)

# Spellings which must not be reintroduced after the Australianised core.
US_SPELLINGS = {
    "aluminum", "apologize", "apologized", "apologizes", "apologizing",
    "behavior", "behaviors", "canceled", "canceling", "center", "centered",
    "centers", "check", "color", "colored", "coloring", "colors", "defense",
    "favor", "favored", "favoring", "favorite", "favorites", "gray", "honor",
    "honored", "honoring", "honors", "jewelry", "kilometer", "kilometers",
    "labor", "license", "liter", "liters", "meter", "meters", "neighbor",
    "neighborhood", "neighborhoods", "neighbors", "offense", "organize",
    "organized", "organizes", "organizing", "realize", "realized", "realizes",
    "realizing", "recognize", "recognized", "recognizes", "recognizing",
    "theater", "theaters", "tire", "tires", "traveled", "traveler", "traveling",
}

AUSTRALIAN_SPELLINGS = {
    "airplane": "aeroplane",
    "counselor": "counsellor",
    "humor": "humour",
    "judgment": "judgement",
    "labeled": "labelled",
    "labeling": "labelling",
    "math": "maths",
    "modeled": "modelled",
    "modeling": "modelling",
    "pajamas": "pyjamas",
    "practicing": "practising",
}

# SUBTLEX is extracted from film and television subtitles.  These entries are
# deliberately omitted from the child-friendly device dictionary.  The list
# also removes subtitle fragments and contractions stripped of punctuation.
BLOCKED_WORDS = set("""
adult affair affairs alcohol alcoholic alley ambush armed army ashes ass asses
assault attack attacked attacking attacks autopsy bastard bastards battle beaten
beating beer beers bitch bitches blade blast bleed bleeding blood bloody bomb bombs
booze bra breasts bullet bullets butcher cannon casino cemetery cheat cheated cheating
cigar cigarettes cocaine cock coffin combat confession convict convicted corpse crap
crime crimes criminal criminals cruel curse custody dead deadly dealer death depressed
depression destruction devil dick disaster dope drown drug drugs drunk drunken dying evil
execution explosion explicit fat filthy firing flesh fool freak freaked freaking freaks
fucker gambling gamble gin grief gross gun guns gunshot guts harmless heroin homicide
hooker horny horror hostage hostages idiot idiots infection insane insult jerk kidnap
kidnapped kidnapping kill killed killer killers killing kills knife liquor lunatic madness
maniac massacre missile missiles mob morgue mortal murder murdered murderer murderers murders
naked nasty naughty nuclear offensive panties pathetic penis pimp pistol poison poker porn
prick prisoner prisoners psycho punishment punk pussy rage raid rape raped ransom revenge
rifle rip ripped robbery robbed rotten sacrifice scandal screw screwed screwing scum sex
sexual sexually sexy shoot shooter shooting shotgun shots sin sins slave slaves slut sober
soldier soldiers spit stab stabbed starving strip suffer suffered suffering suicide sucker
sucks surrender surveillance swear sword terror terrorist terrorists threat threaten threatened
threatening tits torture trauma trapped troops ugly vampire vampires vice victim victims
violence violent virgin vodka war warrior wars weapon weapons weed whack whacked whiskey whore
wicked wound wounded wounds worthless
abuse accused acid betting bitter blown blows brandy chased chaos chemical civilian cocktail
coffin critical crushed damaged defeat demons disturbing drown dynamite explode extreme fighter
fraud grief helpless hostile injured invasion kicking monsters painful patrol penalty punish scar
screams severe skull stroke struggle survival terrified thieves torn tragedy trigger vicious whip
don re ll didn won doesn isn wouldn ain wasn haven couldn em aren shouldn weren th ls hasn ln
la de ho com se sec cos bout hon mon tellin takin til gonna gotta wanna outta kinda anyhow thee
thou thy
""".split())

# Catch inflected profanity while leaving innocent words such as "cockatoo" intact.
BLOCKED_SUBSTRINGS = ("fuck", "motherfuck", "nigger", "nigga", "asshole", "cocksucker")


def core_words(header: Path) -> list[str]:
    text = header.read_text(encoding="utf-8")
    marker = "static const char* const WORDS[BASE_WORD_COUNT] = {"
    body = text.split(marker, 1)[1].split("};", 1)[0]
    return re.findall(r'"([a-z\']+)"', body)


def select_words(source: list[dict], existing: set[str]) -> list[str]:
    start = next(i for i, entry in enumerate(source) if entry["word"] == START_AFTER) + 1
    selected: list[str] = []
    seen = set(existing)
    for entry in source[start:]:
        word = entry["word"]
        if not WORD_RE.fullmatch(word):
            continue
        word = AUSTRALIAN_SPELLINGS.get(word, word)
        if word in seen or word in US_SPELLINGS or word in BLOCKED_WORDS:
            continue
        if any(fragment in word for fragment in BLOCKED_SUBSTRINGS):
            continue
        selected.append(word)
        seen.add(word)
        if len(selected) == TARGET_EXTRA_WORDS - len(PINNED_WORDS):
            selected.extend(PINNED_WORDS)
            return selected
    raise RuntimeError(f"only found {len(selected)} suitable supplementary words")


def render(words: list[str]) -> str:
    lines = [
        "#pragma once",
        "",
        "// Generated by tools/generate_zen_dictionary.py from the ISC-licensed",
        "// SUBTLEX-US spoken-word corpus. Do not edit this list by hand.",
        "namespace zen_dictionary {",
        f"static const size_t PINNED_WORD_COUNT = {len(PINNED_WORDS)};",
        "static const char* const PINNED_WORDS[PINNED_WORD_COUNT] = {",
        "  " + ", ".join(f'\"{word}\"' for word in PINNED_WORDS) + ",",
        "};",
        f"static const size_t EXTRA_WORD_COUNT = {len(words)};",
        "static const char* const EXTRA_WORDS[EXTRA_WORD_COUNT] = {",
    ]
    for index in range(0, len(words), 8):
        chunk = ", ".join(f'"{word}"' for word in words[index:index + 8])
        lines.append(f"  {chunk},")
    lines.extend(["};", "} // namespace zen_dictionary", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="SUBTLEX index.json")
    parser.add_argument("output", type=Path, help="generated header path")
    parser.add_argument(
        "--core",
        type=Path,
        default=Path("examples/companion_radio/zen-overlay/app/zen/WordCompleter.h"),
    )
    args = parser.parse_args()
    source = json.loads(args.source.read_text(encoding="utf-8"))
    existing = core_words(args.core)
    if len(existing) != 2500:
        raise RuntimeError(f"expected 2500 core words, found {len(existing)}")
    args.output.write_text(render(select_words(source, set(existing))), encoding="utf-8")


if __name__ == "__main__":
    main()
