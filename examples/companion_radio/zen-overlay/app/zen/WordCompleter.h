#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ZenWordDictionaryExtra.h"

namespace zen {

// Small, allocation-free prefix completer for conversational text. The word
// list lives in flash and is deliberately independent of KeyboardWidget so it
// can be reused by another UI without pulling in display/input code.
class WordCompleter {
public:
  // Keep enough candidates for a compact editor to fill its one-line hint;
  // callers can still request a smaller result set when space is constrained.
  // Shared candidate capacity also accommodates dotted CLI setting names.
  enum : uint8_t { MAX_SUGGESTIONS = 12, MAX_WORD_LEN = 32 };

  static size_t dictionarySize() { return WORD_COUNT; }
  static const char* wordAt(size_t index) {
    if (index < BASE_WORD_COUNT) return words()[index];
    index -= BASE_WORD_COUNT;
    return index < zen_dictionary::EXTRA_WORD_COUNT
        ? zen_dictionary::EXTRA_WORDS[index] : nullptr;
  }

  struct WordRange {
    size_t start;
    size_t end;
  };

  static WordRange currentWord(const char* text, size_t len, size_t cursor) {
    if (!text) return { 0, 0 };
    if (cursor > len) cursor = len;
    size_t start = cursor;
    while (start > 0 && isWordChar(text[start - 1])) start--;
    size_t end = cursor;
    while (end < len && isWordChar(text[end])) end++;
    return { start, end };
  }

  // Writes up to max_results NUL-terminated matches and returns their count.
  // Empty prefixes intentionally return no words: the picker can still show
  // message placeholders without dumping the whole dictionary.
  static uint8_t suggest(const char* prefix, size_t prefix_len,
                         char results[][MAX_WORD_LEN], uint8_t max_results) {
    if (!prefix || prefix_len == 0 || max_results == 0) return 0;
    if (max_results > MAX_SUGGESTIONS) max_results = MAX_SUGGESTIONS;
    uint8_t count = 0;
    for (size_t i = 0; i < zen_dictionary::PINNED_WORD_COUNT &&
                       count < max_results; i++)
      appendMatch(zen_dictionary::PINNED_WORDS[i], prefix, prefix_len,
                  results, count);
    for (size_t i = 0; i < WORD_COUNT && count < max_results; i++) {
      const char* word = wordAt(i);
      bool duplicate = false;
      for (uint8_t j = 0; j < count; j++)
        if (sameWord(word, results[j])) { duplicate = true; break; }
      if (!duplicate) appendMatch(word, prefix, prefix_len, results, count);
    }
    return count;
  }

private:
  static bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
  static char toLower(char c) { return isUpper(c) ? (char)(c + ('a' - 'A')) : c; }
  static char toUpper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - ('a' - 'A')) : c; }
  static bool isWordChar(char c) {
    c = toLower(c);
    return (c >= 'a' && c <= 'z') || c == '\'';
  }
  static size_t strLength(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
  }
  static bool sameWord(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] && b[i] && toLower(a[i]) == toLower(b[i])) i++;
    return a[i] == '\0' && b[i] == '\0';
  }
  static void appendMatch(const char* word, const char* prefix,
                          size_t prefix_len, char results[][MAX_WORD_LEN],
                          uint8_t& count) {
    if (!startsWith(word, prefix, prefix_len)) return;
    size_t n = strLength(word);
    // An exact match adds nothing and can hide longer, useful completions.
    if (n <= prefix_len) return;
    if (n >= MAX_WORD_LEN) n = MAX_WORD_LEN - 1;
    for (size_t j = 0; j < n; j++) results[count][j] = word[j];
    results[count][n] = '\0';
    if (isUpper(prefix[0])) results[count][0] = toUpper(results[count][0]);
    count++;
  }
  static bool startsWith(const char* word, const char* prefix, size_t prefix_len) {
    for (size_t i = 0; i < prefix_len; i++) {
      if (!word[i] || toLower(word[i]) != toLower(prefix[i])) return false;
    }
    return true;
  }

  static const size_t BASE_WORD_COUNT = 2500;
  static const size_t WORD_COUNT = BASE_WORD_COUNT + zen_dictionary::EXTRA_WORD_COUNT;
  static const char* const* words() {
    // Frequency-ranked conversational English derived from the ISC-licensed
    // SUBTLEX-US spoken corpus, then localised with Australian spellings and
    // everyday vocabulary. Fragments, proper names, profanity and explicit
    // adult/violent terms are omitted for this child-friendly device.
    static const char* const WORDS[BASE_WORD_COUNT] = {
      "you", "the", "to", "it", "that", "and", "of", "what",
      "in", "me", "is", "we", "this", "he", "on", "for",
      "my", "your", "have", "do", "no", "be", "know", "was",
      "not", "can", "are", "all", "with", "just", "get", "here",
      "but", "there", "so", "they", "right", "like", "out", "go",
      "she", "up", "about", "if", "him", "got", "oh", "at",
      "now", "come", "one", "how", "well", "yeah", "her", "want",
      "think", "good", "see", "let", "did", "why", "who", "as",
      "his", "will", "going", "from", "when", "back", "okay", "yes",
      "time", "look", "take", "an", "man", "where", "them", "would",
      "been", "some", "hey", "tell", "or", "us", "had", "were",
      "say", "could", "something", "really", "down", "then", "little", "way",
      "our", "make", "too", "never", "by", "over", "more", "need",
      "mean", "very", "off", "sorry", "give", "has", "thank", "love",
      "said", "am", "people", "please", "sure", "any", "thing", "only",
      "because", "two", "should", "doing", "much", "sir", "maybe", "help",
      "anything", "these", "even", "night", "call", "talk", "nothing", "into",
      "first", "find", "wait", "put", "great", "thought", "day", "work",
      "life", "before", "better", "again", "still", "home", "guy", "those",
      "than", "around", "other", "away", "new", "last", "ever", "stop",
      "keep", "told", "must", "things", "big", "after", "long", "does",
      "always", "their", "everything", "nice", "name", "money", "guys", "feel",
      "believe", "thanks", "old", "place", "fine", "kind", "hello", "lot",
      "years", "made", "leave", "hi", "girl", "hear", "father", "through",
      "every", "bad", "listen", "remember", "three", "boy", "coming", "wrong",
      "might", "stay", "house", "may", "baby", "another", "ok", "dad",
      "wanted", "enough", "talking", "happened", "show", "course", "being", "care",
      "done", "getting", "mind", "left", "ask", "car", "understand", "mother",
      "which", "try", "came", "own", "world", "guess", "next", "else",
      "trying", "someone", "real", "room", "morning", "hold", "woman", "yourself",
      "today", "looking", "mum", "friend", "move", "same", "job", "tonight",
      "went", "son", "best", "saw", "found", "pretty", "ready", "heard",
      "whole", "seen", "together", "minute", "men", "head", "matter", "knew",
      "excuse", "many", "idea", "without", "play", "family", "meet", "most",
      "run", "while", "wife", "once", "live", "somebody", "everybody", "used",
      "use", "myself", "took", "yet", "start", "called", "kid", "tomorrow",
      "happy", "school", "problem", "watch", "bring", "actually", "business", "says",
      "hope", "open", "already", "since", "looks", "sit", "cause", "alone",
      "hard", "wants", "stuff", "turn", "days", "friends", "until", "few",
      "kids", "honey", "gone", "both", "door", "later", "saying", "such",
      "having", "face", "worry", "ago", "five", "second", "brother", "case",
      "thinking", "probably", "beautiful", "hand", "check", "year", "forget", "hit",
      "lost", "minutes", "crazy", "late", "phone", "nobody", "end", "easy",
      "doctor", "shut", "under", "part", "deal", "soon", "four", "anyone",
      "pay", "happen", "true", "each", "supposed", "eat", "mine", "working",
      "town", "afraid", "drink", "exactly", "whatever", "hurt", "knows", "heart",
      "gave", "young", "everyone", "chance", "read", "makes", "number", "taking",
      "change", "anyway", "week", "married", "point", "hands", "police", "word",
      "fun", "wish", "bit", "game", "party", "set", "cut", "comes",
      "sleep", "anybody", "stand", "water", "boys", "trouble", "dear", "couple",
      "gets", "making", "eyes", "break", "story", "far", "times", "close",
      "means", "funny", "goes", "lady", "asked", "walk", "fire", "hours",
      "hate", "rest", "person", "inside", "waiting", "different", "girls", "least",
      "important", "also", "line", "yours", "office", "dinner", "quite", "against",
      "fight", "side", "six", "half", "pick", "question", "ahead", "cool",
      "women", "body", "high", "husband", "reason", "almost", "dog", "buy",
      "truth", "met", "telling", "hot", "anymore", "behind", "started", "speak",
      "bed", "moment", "tried", "shall", "along", "either", "though", "front",
      "sister", "bye", "send", "welcome", "sometimes", "trust", "free", "book",
      "answer", "between", "children", "hurry", "fact", "brought", "clear", "bet",
      "its", "white", "glad", "daughter", "outside", "city", "feeling", "black",
      "seems", "full", "till", "sick", "light", "news", "lose", "wonderful",
      "months", "save", "hour", "country", "needs", "wow", "able", "perfect",
      "running", "child", "order", "living", "sounds", "alive", "food", "gentlemen",
      "luck", "hair", "drive", "promise", "music", "power", "sort", "special",
      "serious", "street", "red", "dance", "hang", "touch", "team", "playing",
      "company", "pull", "plan", "sweet", "ten", "coffee", "lucky", "sound",
      "safe", "date", "leaving", "parents", "himself", "seem", "lives", "air",
      "taken", "picture", "ladies", "sent", "fast", "happens", "perhaps", "catch",
      "ride", "win", "kidding", "top", "scared", "dream", "sign", "meeting",
      "sense", "beat", "control", "drop", "cold", "weeks", "darling", "figure",
      "king", "poor", "throw", "asking", "write", "cannot", "suppose", "small",
      "human", "piece", "boss", "hospital", "past", "calling", "known", "follow",
      "movie", "straight", "christmas", "words", "clean", "kiss", "looked", "feet",
      "evening", "million", "lie", "felt", "moving", "certainly", "step", "learn",
      "hell", "damn", "fall", "questions", "finally", "takes", "class", "quiet",
      "wonder", "law", "become", "worked", "rather", "possible", "goddamn", "unless",
      "mad", "absolutely", "tired", "road", "eye", "except", "somewhere", "explain",
      "less", "none", "loved", "giving", "seeing", "secret", "wear", "worth",
      "act", "careful", "quick", "handle", "pass", "early", "report", "state",
      "busy", "turned", "table", "wake", "works", "broke", "ball", "seven",
      "mouth", "marry", "meant", "fault", "lunch", "expect", "future", "paper",
      "officer", "hotel", "buddy", "thinks", "talked", "blue", "mistake", "ones",
      "wedding", "clothes", "weird", "changed", "court", "floor", "watching", "building",
      "earth", "dude", "others", "longer", "forgot", "finish", "ship", "club",
      "attention", "eight", "worse", "pain", "sing", "blow", "choice", "birthday",
      "stick", "relax", "yesterday", "smart", "boat", "plane", "month", "lovely",
      "given", "train", "fair", "worried", "needed", "sitting", "security", "cover",
      "across", "bag", "terrible", "caught", "song", "spend", "horse", "ring",
      "sell", "return", "personal", "message", "system", "afternoon", "happening", "tough",
      "quit", "count", "box", "missed", "present", "kept", "charge", "information",
      "simple", "middle", "calm", "surprise", "forever", "decided", "dark", "anywhere",
      "miles", "land", "missing", "cute", "lying", "master", "dress", "strong",
      "key", "fix", "interesting", "wearing", "strange", "voice", "rock", "cop",
      "window", "bar", "totally", "interested", "appreciate", "paid", "short", "record",
      "bought", "card", "certain", "college", "fly", "evidence", "bank", "completely",
      "ran", "cops", "test", "history", "finished", "born", "proud", "fish",
      "join", "lead", "smell", "near", "apartment", "enjoy", "letter", "situation",
      "trip", "store", "amazing", "star", "accident", "imagine", "pleasure", "ought",
      "list", "rich", "calls", "service", "entire", "difference", "judge", "ice",
      "lawyer", "instead", "age", "station", "realise", "gold", "seat", "liked",
      "hundred", "summer", "dollars", "standing", "mess", "radio", "hungry", "problems",
      "marriage", "brain", "soul", "forgive", "drunk", "deep", "figured", "likes",
      "girlfriend", "folks", "slow", "private", "during", "attack", "beer", "definitely",
      "stopped", "partner", "walking", "area", "dangerous", "offer", "scene", "third",
      "upset", "bus", "owe", "shoes", "driving", "group", "kick", "joke",
      "fell", "truck", "teach", "ground", "green", "loves", "cash", "forward",
      "honest", "boyfriend", "park", "single", "position", "respect", "broken", "crime",
      "wrote", "public", "grab", "fighting", "art", "favour", "upstairs", "wall",
      "force", "seconds", "jail", "push", "prove", "normal", "protect", "machine",
      "field", "spent", "feels", "speaking", "named", "jump", "starting", "saved",
      "nose", "hide", "sun", "church", "peace", "share", "moved", "picked",
      "thousand", "holding", "fear", "using", "tape", "suit", "pictures", "putting",
      "involved", "gas", "books", "relationship", "neither", "nine", "rules", "bother",
      "especially", "nervous", "whether", "stuck", "round", "dirty", "cat", "breakfast",
      "space", "lived", "prison", "carry", "cry", "smoke", "arm", "film",
      "government", "tree", "foot", "contact", "knock", "agree", "pardon", "gives",
      "gift", "dreams", "hat", "sake", "sweetheart", "board", "seriously", "department",
      "patient", "sad", "wondering", "roll", "beginning", "usually", "grand", "laugh",
      "listening", "doubt", "upon", "double", "twice", "whose", "plenty", "guilty",
      "promised", "fired", "race", "chicken", "bathroom", "spot", "reading", "orders",
      "weekend", "action", "eating", "glass", "type", "experience", "obviously", "wine",
      "press", "difficult", "lots", "rid", "sea", "arms", "flight", "staying",
      "arrest", "neck", "grow", "mention", "favourite", "wind", "sleeping", "notice",
      "admit", "extra", "within", "low", "impossible", "gay", "computer", "angry",
      "bunch", "blame", "pants", "visit", "clock", "tea", "fellow", "kitchen",
      "lay", "hole", "guard", "learned", "smile", "feelings", "fit", "pal",
      "bear", "often", "wild", "silly", "camera", "begin", "reach", "beach",
      "heaven", "lock", "leg", "quickly", "lights", "worst", "played", "plans",
      "bucks", "suddenly", "writing", "track", "teacher", "legs", "river", "dare",
      "burn", "raise", "surprised", "decision", "cross", "cost", "queen", "fresh",
      "innocent", "emergency", "medical", "dancing", "cell", "gotten", "seemed", "bigger",
      "closed", "names", "walked", "hanging", "note", "shop", "sweetie", "band",
      "losing", "price", "steal", "waste", "client", "stole", "crying", "pressure",
      "code", "places", "dogs", "accept", "further", "excellent", "magic", "drinking",
      "keeps", "corner", "consider", "ourselves", "herself", "acting", "locked", "laughing",
      "address", "copy", "tells", "warm", "sold", "pregnant", "hall", "treat",
      "everywhere", "papers", "complete", "cup", "ways", "level", "passed", "witness",
      "eh", "taste", "hardly", "camp", "keeping", "keys", "beg", "duty",
      "interest", "tight", "helping", "bottle", "support", "flying", "decide", "turns",
      "moon", "bottom", "hoping", "conversation", "hero", "asleep", "final", "continue",
      "match", "apologise", "trial", "spirit", "willing", "chair", "risk", "study",
      "possibly", "rain", "above", "cousin", "pulled", "cream", "dropped", "excited",
      "memory", "breathe", "enemy", "huge", "search", "greatest", "drugs", "beauty",
      "lately", "rule", "build", "choose", "cards", "advice", "immediately", "teeth",
      "became", "victim", "coach", "flowers", "showed", "crew", "driver", "heavy",
      "trick", "empty", "comfortable", "destroy", "brothers", "mission", "apart", "pool",
      "dressed", "helped", "checked", "restaurant", "shirt", "faith", "simply", "dig",
      "size", "stars", "movies", "necessary", "themselves", "credit", "blind", "starts",
      "centre", "bridge", "practice", "closer", "discuss", "cars", "mister", "cook",
      "ticket", "strike", "stage", "animal", "bird", "leaves", "sight", "somehow",
      "following", "knowing", "drug", "career", "nature", "responsible", "cake", "famous",
      "nurse", "correct", "breath", "fucked", "games", "allowed", "sky", "bringing",
      "hearing", "singing", "account", "due", "common", "afford", "tie", "bright",
      "allow", "belong", "concerned", "escape", "suspect", "written", "skin", "file",
      "madam", "fill", "operation", "desk", "taught", "pack", "lied", "faster",
      "deserve", "danger", "meat", "command", "stories", "tickets", "paying", "hiding",
      "perfectly", "whoever", "beyond", "student", "dry", "jury", "form", "main",
      "heads", "program", "milk", "held", "horrible", "feed", "natural", "breaking",
      "coat", "settle", "opinion", "terrific", "older", "gentleman", "noticed", "loose",
      "local", "lonely", "shame", "shows", "large", "video", "speed", "built",
      "shower", "oil", "opportunity", "chest", "horses", "biggest", "threw", "bite",
      "wash", "stone", "block", "records", "indeed", "invited", "turning", "draw",
      "attorney", "pretend", "health", "heat", "manager", "guest", "loud", "itself",
      "fantastic", "cares", "shake", "numbers", "lab", "princess", "island", "easier",
      "colour", "earlier", "bell", "suggest", "wet", "pig", "letting", "nowhere",
      "animals", "cheese", "ideas", "downstairs", "monster", "several", "planet", "fellas",
      "eggs", "spoke", "view", "opening", "lines", "insurance", "split", "jealous",
      "arrived", "character", "national", "screaming", "speech", "airport", "hook", "condition",
      "target", "finding", "serve", "incredible", "sugar", "player", "signal", "total",
      "selling", "hill", "football", "page", "justice", "letters", "rough", "hurts",
      "project", "crowd", "meaning", "planning", "pair", "science", "usual", "sees",
      "sooner", "ordered", "subject", "remind", "lies", "strength", "mail", "paint",
      "bedroom", "onto", "neighbourhood", "personally", "finger", "spell", "ghost", "doctors",
      "fake", "release", "weight", "cheap", "market", "pray", "expecting", "unit",
      "signed", "falling", "throat", "lake", "nor", "realised", "director", "agreed",
      "truly", "brilliant", "cab", "powers", "prepared", "candy", "pocket", "legal",
      "aware", "roof", "babe", "slept", "responsibility", "mountain", "base", "ours",
      "firm", "whom", "trade", "romantic", "liar", "fan", "training", "brings",
      "powerful", "whenever", "sending", "language", "purpose", "believed", "bless", "pieces",
      "arrested", "noise", "fancy", "exciting", "genius", "introduce", "forgotten", "rent",
      "familiar", "criminal", "doors", "proof", "vote", "recognise", "stolen", "weather",
      "drinks", "medicine", "lift", "issue", "followed", "buried", "mood", "male",
      "among", "television", "regular", "nights", "opened", "someday", "stomach", "yellow",
      "ate", "nearly", "scare", "village", "prepare", "matters", "pizza", "monkey",
      "sudden", "assume", "heading", "toast", "ears", "fella", "babies", "jacket",
      "thoughts", "social", "travel", "sometime", "property", "expected", "fingers", "remain",
      "bodies", "secretary", "funeral", "magazine", "glasses", "dating", "research", "freedom",
      "add", "damage", "repeat", "handsome", "hired", "prefer", "buying", "society",
      "energy", "crack", "vacation", "chase", "divorce", "stayed", "defence", "rat",
      "picking", "began", "checking", "reasons", "goodness", "post", "confused", "telephone",
      "surgery", "contract", "safety", "tall", "fixed", "professional", "lesson", "tiny",
      "assistant", "points", "understood", "runs", "licence", "model", "gate", "soft",
      "ear", "riding", "staff", "warning", "engine", "planned", "map", "swim",
      "harm", "square", "silver", "brave", "access", "positive", "covered", "female",
      "someplace", "streets", "blew", "weak", "season", "rush", "awesome", "snow",
      "spring", "spread", "champagne", "pounds", "mayor", "demon", "winner", "lips",
      "tongue", "leader", "showing", "permission", "bath", "storm", "spare", "destroyed",
      "tour", "headed", "trees", "students", "ends", "burning", "bones", "kicked",
      "appointment", "mentioned", "score", "shoe", "ocean", "harder", "reality", "shape",
      "survive", "gang", "saving", "style", "farm", "shopping", "clearly", "growing",
      "example", "laid", "answers", "gosh", "rings", "alarm", "plays", "schedule",
      "fortune", "enter", "ended", "patients", "rights", "invite", "obvious", "charges",
      "touched", "interview", "affair", "parts", "wherever", "unbelievable", "focus", "chocolate",
      "borrow", "grew", "grade", "finds", "statement", "investigation", "mate", "load",
      "painting", "throwing", "community", "waited", "loss", "barely", "woods", "changes",
      "details", "yourselves", "exist", "toilet", "chances", "drove", "meal", "dump",
      "disappeared", "member", "shock", "discovered", "failed", "pie", "crash", "artist",
      "sat", "theory", "depends", "bags", "joy", "ruin", "pleased", "traffic",
      "kissed", "wise", "nonsense", "pink", "carrying", "burned", "midnight", "deliver",
      "bread", "officers", "button", "dealing", "original", "hated", "source", "received",
      "hung", "cases", "switch", "charming", "decent", "below", "process", "desert",
      "expensive", "belongs", "particular", "moves", "higher", "lower", "period", "breathing",
      "grandmother", "pride", "thousands", "dollar", "witch", "tip", "jobs", "plant",
      "surely", "sports", "bust", "including", "birth", "joint", "wire", "bull",
      "brains", "towards", "rise", "boring", "ashamed", "sisters", "section", "facts",
      "smells", "clever", "honestly", "success", "garage", "filled", "physical", "connection",
      "complicated", "pulling", "regret", "closet", "giant", "wheel", "parking", "policy",
      "tear", "stranger", "wood", "fate", "juice", "governor", "tied", "faces",
      "awake", "fought", "coast", "pilot", "miracle", "lover", "aboard", "files",
      "based", "cigarette", "grateful", "mighty", "garden", "watched", "forced", "drag",
      "fourth", "scream", "event", "woke", "row", "actor", "grave", "changing",
      "senior", "curious", "flat", "winter", "badly", "shoulder", "scary", "super",
      "priest", "disease", "smoking", "chick", "offered", "closing", "concern", "talent",
      "garbage", "mostly", "attitude", "bone", "recently", "friendly", "egg", "basically",
      "quarter", "engaged", "rooms", "passing", "swing", "available", "slip", "knees",
      "birds", "bike", "hunt", "caused", "taxi", "stood", "likely", "object",
      "hates", "percent", "raised", "guests", "desperate", "dirt", "plate", "negative",
      "cooking", "distance", "tank", "data", "request", "ruined", "hire", "knowledge",
      "golf", "falls", "cow", "dawn", "stock", "equipment", "reports", "conference",
      "rescue", "sale", "claim", "silence", "audience", "warn", "mercy", "proper",
      "create", "universe", "baseball", "soup", "outfit", "slowly", "yard", "grown",
      "loving", "pure", "rate", "dies", "bro", "celebrate", "piano", "uniform",
      "pills", "stealing", "spending", "returned", "location", "duck",
      "doll", "amount", "healthy", "reached", "knocked", "walls", "pen", "steps",
      "younger", "attractive", "notes", "fail", "path", "wanting", "naturally", "happiness",
      "anytime", "eventually", "channel", "belt", "secure", "grandfather", "avoid", "penny",
      "thief", "laughs", "guards", "bride", "mirror", "partners", "dozen", "becomes",
      "direction", "gorgeous", "direct", "odd", "led", "committed", "march", "puts",
      "official", "members", "treated", "effect", "tail", "vision", "secrets", "talks",
      "dust", "trap", "wide", "sharp", "aside", "stairs", "deck", "extremely",
      "unusual", "lousy", "newspaper", "courage", "apple", "terribly", "fishing", "carefully",
      "writer", "pulse", "edge", "illegal", "pity", "protection", "couch", "tests",
      "staring", "created", "screen", "appear", "winning", "precious", "windows", "studio",
      "kissing", "golden", "frightened", "owner", "royal", "intend", "considered", "parties",
      "cast", "popular", "destiny", "silent", "federal", "hearts", "mystery", "nerve",
      "circumstances", "library", "busted", "becoming", "rocks", "practically", "embarrassing", "photo",
      "tower", "shift", "friendship", "maid", "wallet", "package", "range", "flower",
      "results", "steady", "rope", "cleaning", "exact", "image", "vehicle", "turkey",
      "easily", "jungle", "sensitive", "millions", "remembered", "ambulance", "nightmare", "prize",
      "tears", "snake", "families", "cancer", "terms", "orange", "media", "foreign",
      "wasting", "memories", "songs", "material", "expert", "cutting", "advantage", "rude",
      "disappointed", "guarantee", "signs", "committee", "kinds", "downtown", "sandwich", "understanding",
      "marks", "mistakes", "sweat", "political", "panic", "cents", "plain", "performance",
      "stops", "boom", "union", "seats", "hundreds", "fruit", "cable", "separate",
      "underwear", "ancient", "moments", "setting", "rolling", "castle", "delicious", "value",
      "circle", "miserable", "bills", "glory", "squad", "manage", "counting", "bowl",
      "zero", "victory", "stands", "embarrassed", "creature", "deny", "basketball", "mixed",
      "route", "continues", "rare", "yelling", "hidden", "ill", "helps", "directly",
      "progress", "remove", "wave", "gods", "authority", "chain", "highly", "wore",
      "emotional", "hunting", "shadow", "jumped", "skip", "estate", "horn", "appears",
      "basement", "agents", "minds", "pleasant", "mile", "clients", "refuse", "approach",
      "disappear", "speaks", "district", "bug", "rabbit", "champion", "stopping", "proceed",
      "competition", "presence", "leading", "forces", "century", "cure", "capable", "convinced",
      "swell", "warrant", "therefore", "bury", "services", "shine", "diamond", "bat",
      "alert", "chip", "transfer", "thrown", "sentence", "fabulous", "pushed", "nation",
      "butter", "jokes", "reporter", "booth", "successful", "learning", "possibility", "awfully",
      "sand", "desire", "bow", "cage", "wolf", "units", "wing", "exchange",
      "thin", "bored", "pet", "series", "drama", "homework", "hills", "carried",
      "entirely", "zone", "explanation", "spy", "assure", "failure", "hits", "collect",
      "swimming", "print", "launch", "useless", "delivery", "journey", "fever", "photos",
      "sport", "challenge", "loan", "spoken", "routine", "soda", "teaching", "trunk",
      "mask", "leads", "result", "passion", "purse", "served", "argue", "climb",
      "cats", "witnesses", "beef", "recall", "wings", "mental", "cabin", "ships",
      "script", "solid", "article", "education", "salt", "solve", "confidence", "frankly",
      "receive", "metal", "settled", "escaped", "anger", "agency", "detail", "trace",
      "pipe", "wins", "supper", "effort", "studying", "hug", "treatment", "commit",
      "reputation", "intelligence", "ability", "site", "fifth", "trail", "palace", "pushing",
      "stays", "hop", "boots", "owns", "attempt", "houses", "lawyers", "mouse",
      "stronger", "ease", "considering", "ordinary", "presents", "impressed", "customers", "laundry",
      "treasure", "odds", "tricks", "cowboy", "motion", "mall", "virus", "forest",
      "sounded", "trained", "scratch", "breaks", "potential", "fifty", "defend", "contest",
      "plastic", "fashion", "cap", "interrupt", "latest", "convince", "issues", "cheer",
      "arrive", "chose", "supply", "ignore", "nail", "mountains", "league", "vice",
      "figures", "joking", "loaded", "coincidence", "messages", "quality", "title", "impression",
      "particularly", "reasonable", "division", "tiger", "therapy", "museum", "steel", "minister",
      "bound", "standard", "wishes", "yell", "dreaming", "anniversary", "reminds", "shy",
      "firing", "walks", "seek", "chasing", "cancel", "prime", "former", "smooth",
      "socks", "dates", "modern", "surface", "lifetime", "role", "eaten", "chosen",
      "gym", "motel", "enjoyed", "collection", "device", "heck", "noon", "blowing",
      "sons", "reward", "degrees", "lets", "bothering", "bars", "dumped", "iron",
      "cameras", "express", "cookies", "assignment", "tunnel", "highway", "insist", "guide",
      "slide", "specific", "wrap", "cleaned", "wagon", "prom", "lack", "packed",
      "exercise", "defendant", "kit", "background", "ringing", "clue", "suits", "concert",
      "temple", "ranch", "designed", "planes", "foolish", "agreement", "darkness", "flag",
      "tent", "rotten", "term", "remains", "alien", "provide", "touching", "patch",
      "snap", "believes", "imagination", "bail",
      "colours", "coloured",
      "colouring", "favourites", "favours", "favoured", "favouring", "honours", "honoured", "honouring",
      "centres", "centred", "theatres", "metre", "metres", "litre", "litres", "kilometre",
      "kilometres", "neighbour", "neighbours", "realises", "realising", "organise", "organised", "organising",
      "recognised", "recognising", "apologised", "apologising", "behaviour", "labour", "travelled", "travelling",
      "traveller", "cancelled", "cancelling", "jewellery", "offence", "grey", "tyre", "tyres",
      "aluminium", "cheque", "aussie", "arvo", "brekkie", "servo", "mozzie", "footy",
      "sunnies", "bikkie", "barbie", "snag", "chook", "ute", "esky", "thongs",
      "swimmers", "togs", "boardies", "trackies", "jumper", "doona", "loo", "rubbish",
      "bin", "footpath", "chemist", "postie", "tradie", "sparky", "firie", "ambo",
      "rego", "avo", "cuppa", "takeaway", "lolly", "lollies", "reckon", "cheers",
      "ta", "heaps", "keen", "beaut", "dinkum", "bush", "outback", "creek",
      "paddock", "scrub", "gumtree", "eucalyptus", "wattle", "billy", "kangaroo", "roo",
      "wallaby", "wombat", "koala", "platypus", "kookaburra", "cockatoo", "galah", "magpie",
      "possum", "echidna", "quokka", "dingo", "emu", "vegemite", "lamington", "pavlova",
      "damper", "barramundi", "kindy", "canteen", "tuckshop", "mobile", "torch", "holiday",
      "caravan", "petrol", "indicator", "bonnet", "windscreen", "bushwalk", "honour", "theatre"
    };
    return WORDS;
  }
};

} // namespace zen
