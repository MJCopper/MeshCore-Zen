#pragma once

#include "../zen/SuggestionPreview.h"

// Owns completion-provider callbacks and their cached presentation. The
// editor controller decides when text changed; renderers only read the cache.
template <typename Editor, int WordLength, int ListLength>
class KeyboardCompletionSession {
 public:
  using RefreshFn = void (*)(Editor&, void*);
  using PreviewFn = bool (*)(const Editor&, void*, char*, size_t, char*, size_t,
                             char*, size_t);

 private:
  RefreshFn _refresh = nullptr;
  void* _refresh_context = nullptr;
  PreviewFn _preview_fn = nullptr;
  void* _preview_context = nullptr;
  zen::SuggestionPreview<WordLength, ListLength> _preview;

 public:
  void reset() {
    _refresh = nullptr;
    _refresh_context = nullptr;
    _preview_fn = nullptr;
    _preview_context = nullptr;
    _preview.invalidate();
  }
  void setRefresh(RefreshFn fn, void* context) {
    _refresh = fn;
    _refresh_context = context;
  }
  void setPreview(PreviewFn fn, void* context) {
    _preview_fn = fn;
    _preview_context = context;
    _preview.invalidate();
  }
  bool hasRefresh() const { return _refresh != nullptr; }
  void populate(Editor& editor) const {
    if (_refresh) _refresh(editor, _refresh_context);
  }
  void invalidate() { _preview.invalidate(); }
  bool refreshPreview(const Editor& editor, uint32_t revision, bool blocked) {
    if (_preview.revision == revision) return _preview.available;
    _preview.clear(revision);
    if (!_preview_fn || blocked) return false;
    _preview.available = _preview_fn(
        editor, _preview_context, _preview.word, sizeof(_preview.word),
        _preview.suffix, sizeof(_preview.suffix), _preview.candidates,
        sizeof(_preview.candidates));
    return _preview.available;
  }
  const zen::SuggestionPreview<WordLength, ListLength>& preview() const {
    return _preview;
  }
};
