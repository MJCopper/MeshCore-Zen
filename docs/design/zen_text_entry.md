# Zen text-entry framework

Zen uses one fixed-allocation editor shared by every screen. `KeyboardWidget`
is the compatibility facade; its behaviour is divided into the following
modules:

- `TextBuffer` owns byte limits and UTF-8-safe editing.
- `KeyboardInputState` and `KeyboardInputController` own on-screen navigation,
  multi-tap state and normalized editor-action handling.
- `KeyboardPredictionState` and `PredictionSession` own active T9 state and
  rank/cache contextual and dictionary results.
- `SuggestionModel`, `SuggestionPreview` and `KeyboardCompletionSession` own
  popup providers, cached inline suggestions and their invalidation lifecycle.
- `TextEditorTypes` defines editor profiles and source-independent actions.
- `CardKBEditorInput` translates CardKB events without owning I2C or power.
- `KeyboardLayout` calculates geometry shared by OLED and E-ink rendering.
- `TextPreviewRenderer`, `CompactEditorRenderer` and `KeyboardGridRenderer`
  keep each presentation independent while `KeyboardRenderer` coordinates it.
- `EmojiPicker` owns emoji selection.
- `MessageDraftStore` owns RAM-only storage and `MessageComposeSession` binds
  load, save and clear operations to the selected conversation.

## Profiles

Message and Quick Reply fields enable sentence case, emoji, predictive T9 and
context ranking. Console fields use the command provider. Literal and
credential fields do not enable prediction, emoji or message placeholders.

All storage remains fixed-size. Prediction is refreshed after editor input and
cached for rendering; rendering does not repeatedly search the dictionaries.
CardKB remains literal QWERTY input and automatically selects the compact
presentation, while the on-screen keyboard follows the configured ABC/T9 mode.
