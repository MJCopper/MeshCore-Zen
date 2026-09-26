#pragma once

#include <helpers/ui/ZenDisplayDriver.h>
#include <Arduino.h>
#include "PopupMenu.h"
#include "EmojiPicker.h"
#include "icons.h"   // mini-icons for the special-key row (⇧ ⌫ ⎵ ✓)
#include "KeyboardLayout.h"
#include "KeyboardInputActions.h"
#include "KeyboardCompletionSession.h"
#include "../ZenPrefs.h"
#include "../Features.h"
#include "../zen/SentenceCase.h"
#include "../zen/TextBuffer.h"
#include "../zen/TextEditorTypes.h"
#include "../zen/KeyboardInputState.h"
#include "../zen/KeyboardPredictionState.h"
#include "../zen/SuggestionModel.h"
#include "../zen/SuggestionPreview.h"
#if ZEN_FEATURE_AUTOCOMPLETE
#include "../zen/T9Predictor.h"
#include "../zen/ContextPredictor.h"
#include "../zen/PredictionSession.h"
#endif

// Layout constants shared by all keyboard users.
// Two pages: letters (page 0) and symbols (page 1), toggled by the "#@"/"abc"
// special key. Space lives only on the ⎵ special key now, so the freed grid
// slot on page 0 holds the comma; punctuation is grouped as . , ! ?
static const int KB_PAGES      = 2;
static const char KB_CHARS[KB_PAGES][4][10] = {
  { // page 0 — letters + digits
    {'a','b','c','d','e','f','g','h','i','j'},
    {'k','l','m','n','o','p','q','r','s','t'},
    {'u','v','w','x','y','z','.',',','!','?'},
    {'1','2','3','4','5','6','7','8','9','0'},
  },
  { // page 1 — symbols + digits (ASCII only — one byte per key)
    {'@','#','&','*','(',')','-','_','+','='},
    {'/','\\',':',';','\'','"','<','>','[',']'},
    {'{','}','|','~','^','$','%','`',',','.'},
    {'1','2','3','4','5','6','7','8','9','0'},
  },
};
static const int KB_ROWS_CHAR  = 4;
static const int KB_COLS_CHAR  = 10;
static const int KB_SPECIAL    = 6;   // ⇧ ⎵ ⌫ 🙂 #@/abc ✓

// T9 layout (Settings › Keyboard). Message fields use predictive input on keys
// 2-9; key 1, the symbols page, and literal fields retain multi-tap. The page
// toggle also exposes an `abc` multi-tap fallback for words outside the
// dictionary. Zero is the first character on the symbols page's bottom-right
// key. Space/backspace/etc. live on the special row shared with the ABC layout.
static const int KB_T9_ROWS = 3;
static const int KB_T9_COLS = 3;
static const uint32_t KB_T9_TIMEOUT_MS = 800;
static const char* const KB_T9_GROUPS[KB_PAGES][9] = {
  { ".,!?'-", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz" },   // page 0 — letters
  { "@#&", "*()", "-_+", "=/\\", ":;'\"", "<>[]", "{}|~", "^$%`", "0,." }, // page 1 — symbols + zero
};


// Buffer cap for typed text, in bytes. Matches MeshCore's MAX_TEXT_LEN
// (10*CIPHER_BLOCK_SIZE = 160) so a full-length message can be composed; each
// field passes its own smaller max to begin() where its store is smaller.
static const int KB_MAX_LEN    = 160;

// Longest preview line we render per row, in CHARACTERS. Caps the per-line
// stack buffers so a very wide display (small font → many chars per line)
// can't overrun them.
static const int KB_PREVIEW_CAP = 46;
// The same cap in BYTES. Unicode scalar values, including emoji, can occupy
// four UTF-8 bytes even though each still consumes one display cell.
static const int KB_PREVIEW_BYTES = KB_PREVIEW_CAP * 4;

// ── UTF-8 helpers ────────────────────────────────────────────────────────────
// The EN-US grid is ASCII, but emoji and externally supplied text can contain
// UTF-8. Editing therefore remains codepoint-aware.

// Apply Shift/caps to ASCII letters while preserving any other UTF-8 supplied
// by an existing field value.
static void kbApplyCapsUtf8(const char* in, bool caps, char* out, size_t out_size) {
  zen::utf8::applyAsciiCaps(in, caps, out, out_size);
}

// Codepoint count of a UTF-8 string (byte length overcounts once a 2-byte
// alphabet is involved — this is what T9 cycling needs instead of strlen()).
static int kbUtf8Len(const char* s) {
  return zen::utf8::length(s);
}

// Extract the idx-th codepoint of a UTF-8 string as its own NUL-terminated
// UTF-8 bytes in `out` (>= 5 bytes). Empty string if idx is out of range.
static void kbUtf8CharAt(const char* s, int idx, char* out) {
  zen::utf8::charAt(s, idx, out);
}

// Byte width of the LAST codepoint in buf[0..len) — for backspace and the T9
// in-place replace, which must remove/overwrite a whole codepoint, not one
// byte (a lone trailing continuation byte would otherwise corrupt a 2-byte
// character). Capped at 4 (UTF-8's max), though this codebase's alphabets are
// all <= 2 bytes today.
static int kbUtf8LastCharBytes(const char* buf, int len) {
  return zen::utf8::previousWidth(buf, len);
}

// Byte width of the codepoint STARTING at buf[pos] (pos in [0,len)) — the
// forward counterpart to kbUtf8LastCharBytes, for moving the edit cursor
// right by one whole codepoint instead of one byte.
static int kbUtf8CharBytesAt(const char* buf, int pos, int len) {
  return zen::utf8::widthAt(buf, pos, len);
}

static const int KB_PH_MAX     = 20;  // max placeholders in list (PopupMenu::PM_MAX_ITEMS=24 is the hard ceiling)
static const int KB_PH_LEN     = 32;  // max placeholder string length incl. null -- sized for the longest
                                       // CLI-command candidate (AdminScreen), not just the short {x} tokens
static const int KB_PH_VISIBLE = 3;   // items shown at once in overlay

struct KeyboardWidget;
// Optional hook: if set, called right before the placeholder picker opens so
// the caller can repopulate the list contextually (e.g. AdminScreen's CLI
// command autocomplete, filtered by what's already typed). When set, picking
// an entry also replaces the in-progress word (the text since the last
// space) instead of appending it -- true completion, not insertion. Fields
// that don't set this keep the original static-list, append-only behaviour.
typedef void (*PlaceholderRefreshFn)(KeyboardWidget& kb, void* ctx);
typedef bool (*CompletionPreviewFn)(const KeyboardWidget& kb, void* ctx,
                                    char* word, size_t word_size,
                                    char* suffix, size_t suffix_size,
                                    char* candidates, size_t candidates_size);

struct KeyboardWidget : public zen::TextBuffer<KB_MAX_LEN>,
                        public zen::KeyboardInputState,
                        public zen::KeyboardPredictionState {
  // Set by render() every time it's actually called; UITask clears it before
  // curr->render() each frame (beginFrame()) so it reflects only "was the
  // keyboard the thing on screen this frame" -- lets the alert overlay (new
  // message toast) skip drawing over a full-screen keyboard, regardless of
  // which screen (Messages/Bot/Settings/Admin/...) currently owns it.
  bool _visible = false;
  bool _external_keyboard_connected = false;
  bool _emoji_enabled = false;
  bool _sentence_case_enabled = false;
  zen::EditorProfile _profile = zen::EditorProfile::LITERAL;
  EmojiPicker _emoji_picker;
  void beginFrame() { _visible = false; }
  bool isVisible() const { return _visible; }
  void setExternalKeyboardConnected(bool connected) { _external_keyboard_connected = connected; }
  void setSentenceCase(bool enabled) { _sentence_case_enabled = enabled; }
  bool isCompact() const { return _external_keyboard_connected; }

  // True while the plain letter/symbol grid is the active input surface --
  // showing, no popup open, not mid cursor-reposition.
  // Used by CardKB's Compact-mode handling (UITask::pollCardKB()) to tell
  // "grid navigation" apart from every other state arrows/Enter already mean
  // something else in (those all render their own visible feedback, so they
  // don't need Compact's special-casing).
  bool inPlainGridState() const {
    return isVisible() && !_ph_menu.active && !_emoji_picker.active() &&
           !cursor_mode;
  }
  void setEmojiEnabled(bool enabled) { _emoji_enabled = enabled; }

  zen::SuggestionModel<KB_PH_MAX, KB_PH_LEN> _suggestions;
  PopupMenu _ph_menu;
  KeyboardCompletionSession<KeyboardWidget, KB_PH_LEN, 96> _completion;
  uint32_t _text_revision = 0;
  void setPlaceholderRefresh(PlaceholderRefreshFn fn, void* ctx,
                             const char* title = "Placeholder:", bool append_space = false) {
    _completion.setRefresh(fn, ctx);
    _suggestions.title = title;
    _suggestions.append_space = append_space;
  }
  void setCompletionRange(int start, int end) {
    _suggestions.setRange(start, end, len);
  }
  void setCompletionPreview(CompletionPreviewFn fn, void* ctx) {
    _completion.setPreview(fn, ctx);
  }

  void invalidateTextPresentation() {
    _text_revision++;
    _completion.invalidate();
    invalidateT9Cache();
  }

  bool refreshCompletionPreview() {
    return _completion.refreshPreview(*this, _text_revision,
                                      predictiveT9Active());
  }

  // Live setting lookup — set once by UITask::begin(). NULL only in tests/tools
  // that construct a KeyboardWidget standalone, in which case isT9() defaults
  // to ABC.
  ZenPrefs* prefs = nullptr;
  bool isT9() const { return prefs && prefs->keyboard_type == 1; }

#if ZEN_FEATURE_AUTOCOMPLETE
  bool predictiveT9Ready() const {
    return _predictive_t9_enabled && isT9() && !isCompact() &&
           page == 0 && !_t9_literal_mode;
  }
  bool predictiveT9Active() const { return predictiveT9Ready() && _t9_digit_count > 0; }

  void invalidateT9Cache() {
    _prediction.invalidate();
  }

  void resetT9Prediction() {
    _t9_digit_count = 0;
    _t9_digits[0] = '\0';
    _t9_candidate[0] = '\0';
    _t9_no_match = false;
    _t9_capacity_limited = false;
    _t9_word_start = _t9_word_end = cursor_pos;
    invalidateT9Cache();
  }

  uint8_t getT9Candidates(
      char out[][zen::WordCompleter::MAX_WORD_LEN], uint8_t max_results) {
    int capacity = max_len - _t9_word_start - (len - _t9_word_end);
    return _prediction.suggest(buf, (size_t)_t9_word_start, _t9_digits,
        _t9_digit_count, _context_prediction_enabled, _t9_predict_caps,
        capacity, _t9_suggest, out, max_results);
  }

  bool replaceT9Text(const char* word) {
    if (!replace(_t9_word_start, _t9_word_end, word ? word : "")) return false;
    _t9_word_end = cursor_pos;
    return true;
  }

  bool showT9Candidate(const char* word) {
    if (!word) return false;
    int tail_len = len - _t9_word_end;
    if (_t9_word_start + (int)strlen(word) + tail_len > max_len) return false;
    snprintf(_t9_candidate, sizeof(_t9_candidate), "%s", word);
    size_t visible = zen::T9Predictor::prefixBytes(word, _t9_digit_count);
    char prefix[zen::WordCompleter::MAX_WORD_LEN];
    if (visible >= sizeof(prefix)) visible = sizeof(prefix) - 1;
    memcpy(prefix, word, visible);
    prefix[visible] = '\0';
    _t9_no_match = false;
    return replaceT9Text(prefix);
  }

  void commitT9Prediction() {
    if (_t9_digit_count && !_t9_no_match && _t9_candidate[0]) {
      if (replaceT9Text(_t9_candidate) && !_t9_suggest)
        zen::T9Predictor::remember(_t9_candidate);
    }
    resetT9Prediction();
  }

  bool acceptT9WordAndSpace() {
    if (!predictiveT9Active() || _t9_no_match) return false;
    commitT9Prediction();
    insert(" ");
    return true;
  }

  bool acceptT9WordAndSentence() {
    if (!predictiveT9Active() || _t9_no_match) return false;
    commitT9Prediction();
    static const char ending[] = ". ";
    insert(ending);
    return true;
  }

  // Accept the normal completion preview shown after editing an existing word.
  // This state has no active T9 digit sequence, so it must be handled separately
  // from acceptT9WordAndSpace()/Sentence().
  bool acceptPreviewCompletion(const char* ending) {
    if (predictiveT9Active() || !refreshCompletionPreview()) return false;
    const char* suffix = _completion.preview().suffix;
    int suffix_len = strlen(suffix);
    int ending_len = ending ? strlen(ending) : 0;
    if (suffix_len > 0 && insert(suffix) && ending_len > 0) insert(ending);
    return true;  // a visible prediction always consumes Back, even at capacity
  }

  const char* t9GhostSuffix() const {
    if (!predictiveT9Active() || _t9_no_match || !_t9_candidate[0]) return "";
    size_t visible = zen::T9Predictor::prefixBytes(_t9_candidate, _t9_digit_count);
    size_t length = strlen(_t9_candidate);
    return visible < length ? _t9_candidate + visible : "";
  }

  bool appendT9Digit(char digit) {
    _t9_capacity_limited = false;
    if (!predictiveT9Ready() || _t9_digit_count + 1 >= sizeof(_t9_digits)) return false;
    bool starting = _t9_digit_count == 0;
    if (starting) {
      _t9_word_start = _t9_word_end = cursor_pos;
      _t9_predict_caps = caps || (_sentence_case_enabled &&
          zen::SentenceCase::shouldCapitalize(buf, (size_t)cursor_pos));
    }
    _t9_digits[_t9_digit_count++] = digit;
    _t9_digits[_t9_digit_count] = '\0';
    char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN];
    uint8_t count = getT9Candidates(matches, zen::WordCompleter::MAX_SUGGESTIONS);
    if (count == 0 && suggestT9(
          _t9_digits, _t9_digit_count, matches, 1, zen::WordCompleter::MAX_WORD_LEN - 1) > 0) {
      // A word exists but cannot fit. Preserve the text and previous prediction.
      _t9_digits[--_t9_digit_count] = '\0';
      _t9_capacity_limited = true;
      invalidateT9Cache();
      return true;
    }
    if (count == 0 || !showT9Candidate(matches[0])) {
      // Retain the complete digit sequence and remove the provisional word.
      // Hold Enter now offers an explicit literal-entry recovery instead of
      // silently discarding the final key.
      replaceT9Text("");
      _t9_candidate[0] = '\0';
      _t9_no_match = true;
      return true;
    }
    if (starting && caps && !caps_lock) caps = false;
    return true;
  }

  bool backspaceT9Prediction() {
    _t9_capacity_limited = false;
    if (!predictiveT9Active()) return false;
    _t9_digit_count--;
    _t9_digits[_t9_digit_count] = '\0';
    if (_t9_digit_count == 0) {
      replaceT9Text("");
      resetT9Prediction();
      return true;
    }
    char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN];
    uint8_t count = getT9Candidates(matches, zen::WordCompleter::MAX_SUGGESTIONS);
    if (count) showT9Candidate(matches[0]);
    else {
      replaceT9Text("");
      _t9_candidate[0] = '\0';
      _t9_no_match = true;
    }
    return true;
  }
#else
  bool predictiveT9Ready() const { return false; }
  bool predictiveT9Active() const { return false; }
  void resetT9Prediction() { }
  void commitT9Prediction() { }
  bool acceptT9WordAndSpace() { return false; }
  bool acceptT9WordAndSentence() { return false; }
  bool acceptPreviewCompletion(const char*) { return false; }
  const char* t9GhostSuffix() const { return ""; }
  bool appendT9Digit(char) { return false; }
  bool backspaceT9Prediction() { return false; }
#endif

  void setPredictiveT9(bool enabled) {
#if ZEN_FEATURE_AUTOCOMPLETE
    _predictive_t9_enabled = enabled;
#else
    (void)enabled;
#endif
  }

  void setContextPrediction(bool enabled) {
#if ZEN_FEATURE_AUTOCOMPLETE
    _context_prediction_enabled = enabled;
    invalidateT9Cache();
#else
    (void)enabled;
#endif
  }

#if ZEN_FEATURE_AUTOCOMPLETE
  void setT9Provider(T9SuggestFn provider) {
    _t9_suggest = provider;
    invalidateT9Cache();
  }
  uint8_t suggestT9(const char* digits, size_t count,
                    char out[][zen::WordCompleter::MAX_WORD_LEN],
                    uint8_t max_results, size_t max_bytes) {
    return _t9_suggest ? _t9_suggest(digits, count, out, max_results, max_bytes) :
        zen::T9Predictor::suggest(digits, count, out, max_results, max_bytes);
  }
#endif

  int gridRows() const { return isT9() ? KB_T9_ROWS : KB_ROWS_CHAR; }
  int gridCols() const { return isT9() ? KB_T9_COLS : KB_COLS_CHAR; }

  int totalPages() const { return KB_PAGES; }
  bool pageIsSymbols(int pg) const { return pg == 1; }

  const char* cellStr(int r, int c) const {
    static char single[2];
    single[0] = KB_CHARS[pageIsSymbols(page) ? 1 : 0][r][c];
    single[1] = '\0';
    return single;
  }

  const char* t9GroupStr(int cell) const {
    return KB_T9_GROUPS[pageIsSymbols(page) ? 1 : 0][cell];
  }

  enum Result { NONE, DONE, CANCELLED };

  zen::EditorProfile profile() const { return _profile; }

  void configureProfile(zen::EditorProfile profile) {
    _profile = profile;
    const zen::EditorFeatures features = zen::featuresFor(profile);
    setSentenceCase(features.sentence_case);
    setEmojiEnabled(features.emoji);
    setPredictiveT9(features.predictive_t9);
    setContextPrediction(features.context_prediction);
  }

  void beginProfile(zen::EditorProfile profile, const char* initial = "",
                    int max = KB_MAX_LEN) {
    begin(initial, max);
    configureProfile(profile);
  }

  void begin(const char* initial = "", int max = KB_MAX_LEN) {
    reset(initial, max);
    cursor_mode = false;
    _emoji_enabled = false;
    _sentence_case_enabled = false;
    _emoji_picker.close();
    row = col = 0;
    page = 0;
    caps = false;
    caps_lock = false;
    t9_cell = -1;
    t9_cycle = 0;
#if ZEN_FEATURE_AUTOCOMPLETE
    _predictive_t9_enabled = false;
    _context_prediction_enabled = false;
    _t9_suggest = nullptr;
    _t9_literal_mode = false;
    _t9_predict_caps = false;
    resetT9Prediction();
#endif
    _ph_menu.active = false;
    _completion.reset();
    _suggestions.reset(cursor_pos);
    invalidateTextPresentation();
    // Placeholders are supplied by the owning screen for the current context.
  }

  Result handleAction(zen::EditorAction action) {
    Result result = NONE;
    switch (action) {
      case zen::EditorAction::NAV_UP: result = handleInputImpl(KEY_UP); break;
      case zen::EditorAction::NAV_DOWN: result = handleInputImpl(KEY_DOWN); break;
      case zen::EditorAction::NAV_LEFT: result = handleInputImpl(KEY_LEFT); break;
      case zen::EditorAction::NAV_RIGHT: result = handleInputImpl(KEY_RIGHT); break;
      case zen::EditorAction::ACTIVATE: result = handleInputImpl(KEY_ENTER); break;
      case zen::EditorAction::HOLD_ACTIVATE: result = handleInputImpl(KEY_CONTEXT_MENU); break;
      case zen::EditorAction::CANCEL: result = handleInputImpl(KEY_CANCEL); break;
      case zen::EditorAction::DOUBLE_CANCEL: result = handleInputImpl(KEY_DOUBLE_CANCEL); break;
      case zen::EditorAction::BACKSPACE: result = handleInputImpl(0x08); break;
      case zen::EditorAction::SUBMIT: result = handleInputImpl(KEY_KB_ENTER); break;
      case zen::EditorAction::OPEN_SUGGESTIONS:
        openPlaceholders(); break;
      case zen::EditorAction::OPEN_EMOJI:
        openEmojiPicker(); break;
      case zen::EditorAction::NONE:
      default: break;
    }
    invalidateTextPresentation();
    return result;
  }

  // Insert one grid glyph at cursor_pos, applying Shift/caps-lock.
  void insertGlyph(const char* one, bool use_caps) {
    if (_sentence_case_enabled && one && one[0] >= 'a' && one[0] <= 'z' &&
        zen::SentenceCase::shouldCapitalize(buf, (size_t)cursor_pos))
      use_caps = true;
    char shown[5];
    kbApplyCapsUtf8(one, use_caps, shown, sizeof(shown));
    insert(shown);
  }

  // Insert an arbitrary UTF-8 sequence at the cursor. Message emoji and future
  // Unicode pickers share this byte-safe path; max_len remains the encoded
  // message limit, while cursor movement/backspace remain codepoint-aware.
  bool insertUtf8(const char* text) {
    return insert(text);
  }

  // Insert one character typed literally on an external keyboard (CardKB or
  // similar), as opposed to committed off the on-screen grid. The source
  // already sends the correct case, so no Shift/caps-lock is re-applied.
  //
  // This is the single translation point for external-keyboard input: today
  // it's the identity mapping (CardKB is a Latin QWERTY, so what it sends is
  // what gets typed, regardless of the on-screen grid's ABC/T9 setting --
  // that only governs grid navigation). To support relabelled keycaps later,
  // map `c` to that layout's codepoint here and
  // hand the resulting UTF-8 to insertGlyph() -- everything downstream already
  // works in codepoints, not bytes. Such a layout belongs on its own setting,
  // a dedicated physical-layout setting: it describes the keycaps, which are
  // independent of the on-screen grid. Digits/punctuation
  // should keep passing through unmapped.
  void insertTyped(char c) {
    t9_cell = -1;   // otherwise a same-cell T9 tap within KB_T9_TIMEOUT_MS would
                    // overwrite this character instead of inserting a new one
    char one[2] = { c, '\0' };
    insertGlyph(one, false);
  }

  void clearPlaceholders() { _suggestions.clear(); }

  void addPlaceholder(const char* ph) {
    _suggestions.add(ph);
  }

  // Opens the completion/placeholder picker directly. Used by Hold Enter in
  // message editors and CardKB's Compact mode (plain Tab).
  bool openPlaceholders() {
    if (!inPlainGridState()) return false;
    t9_cell = -1;   // finalize any pending multi-tap cycle -- the pick below moves the
                    // cursor, so a later same-cell tap must not "continue" onto it
    _suggestions.range_set = false;
    _ph_t9_actions = false;
#if ZEN_FEATURE_AUTOCOMPLETE
    if (predictiveT9Active()) {
      clearPlaceholders();
      setCompletionRange(_t9_word_start, _t9_word_end);
      if (_t9_no_match) {
        _ph_t9_actions = true;
        addPlaceholder("Spell word");
        addPlaceholder("Cancel word");
      } else {
        char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN];
        uint8_t count = getT9Candidates(matches, zen::WordCompleter::MAX_SUGGESTIONS);
        for (uint8_t i = 0; i < count; i++) addPlaceholder(matches[i]);
      }
    } else
#endif
    _completion.populate(*this);
    _ph_menu.begin(_ph_t9_actions ? "No match" : _suggestions.title, KB_PH_VISIBLE);
    for (int i = 0; i < _suggestions.count; i++)
      _ph_menu.addItem(_suggestions.items[i]);
    return true;
  }

  bool openEmojiPicker() {
    if (!_emoji_enabled || !inPlainGridState()) return false;
    t9_cell = -1;
    commitT9Prediction();
    _emoji_picker.open();
    return true;
  }

  // Moves the text cursor directly (LEFT/RIGHT one codepoint, UP/DOWN to
  // start/end) without engaging cursor_mode's grid-boundary wrap (continuing
  // into the special row / row 0 once already at an end) -- that wrap exists
  // so a physical-button user can see where they land back on the grid, which
  // doesn't apply here: CardKB's Compact mode never navigates the grid at
  // all, so there's no grid position to wrap into. See pollCardKB().
  void moveCursorDirect(char key) {
    t9_cell = -1;   // same invariant every other cursor-moving path keeps: a pending
                    // multi-tap cycle must not resume against a moved cursor and
                    // overwrite an unrelated character. (cursor_mode gets this for
                    // free -- its KEY_UP entry point already clears t9_cell.)
    int previous = cursor_pos;
    if (key == KEY_LEFT) movePrevious();
    else if (key == KEY_RIGHT) moveNext();
    else if (key == KEY_UP) moveHome();
    else if (key == KEY_DOWN) moveEnd();
    if (cursor_pos != previous) invalidateTextPresentation();
  }

  int render(ZenDisplayDriver& display);
  void renderTextPreview(ZenDisplayDriver& display, const KeyboardLayout& layout,
                         bool compact, bool no_match_hint, int preview_lines,
                         const char* suffix);
  void renderCompactEditor(ZenDisplayDriver& display, const KeyboardLayout& layout,
                           bool has_completion);
  void renderKeyboardGrid(ZenDisplayDriver& display, const KeyboardLayout& layout,
                          int rows, int columns);

  Result handleInputImpl(char c);

  // Compatibility facade for existing screens. All sources pass through this
  // boundary so presentation caches are invalidated by input rather than from
  // render(), keeping rendering deterministic and idle-cost free.
  Result handleInput(char c) {
    zen::EditorAction action = keyboardinput::actionForKey(c);
    if (action != zen::EditorAction::NONE) return handleAction(action);
    Result result = handleInputImpl(c);  // literal text from CardKB
    invalidateTextPresentation();
    return result;
  }
};

#include "TextPreviewRenderer.inl"
#include "CompactEditorRenderer.inl"
#include "KeyboardGridRenderer.inl"
#include "KeyboardRenderer.inl"
#include "KeyboardInputController.inl"
