#pragma once
#include <string.h>
#include "PopupMenu.h"
#include "../RadioPresets.h"
#include "../ZenPrefs.h"

// Bounded, offline preset picker. Sixteen built-ins per page leave room for
// save, four user presets, delete and both navigation rows in the shared menu.
// User presets retain their existing four-RF-parameter format.
struct RadioPresetPicker {
  // Explicit user selection also applies optional built-in network metadata.
  struct Target {
    float*   freq;
    float*   bw;
    uint8_t* sf;
    uint8_t* cr;
  };

  // What the owning screen should do after a SELECTED item is handled.
  enum Result {
    NONE,        // nothing to do (sub-menu opened, or a no-op row)
    START_SAVE,  // open the keyboard to name a new preset, then call save()
    APPLIED,     // target fields changed — apply to the radio + mark dirty
    DELETED      // a saved preset was removed — mark dirty + alert
  };

  PopupMenu menu;
  uint8_t   user_slot[ZenPrefs::USER_RADIO_PRESET_MAX];   // popup row → ZenPrefs slot
  int       user_count = 0;
  bool      saving   = false;   // keyboard is open to name a new preset
  bool      deleting = false;   // menu is showing the delete sub-list
  int8_t    confirm_slot = -1;  // slot awaiting destructive-action confirmation
  uint8_t   page = 0; // Fits the existing ARM object padding.
  static constexpr int PAGE_SIZE = 16;
  static_assert(PAGE_SIZE + ZenPrefs::USER_RADIO_PRESET_MAX + 4 <= PopupMenu::PM_MAX_ITEMS,
                "Preset page exceeds popup capacity");

  static bool matches(const Target& t, float freq, float bw, uint8_t sf, uint8_t cr) {
    return radioParamsMatchPreset(*t.freq, *t.bw, *t.sf, *t.cr, freq, bw, sf, cr);
  }

  static bool matchesBuiltin(ZenPrefs* p, const Target& t, const RadioPreset& r) {
    return p && matches(t, r.freq, r.bw, r.sf, r.cr) &&
      (!r.path_hash_size || p->getRadioPrefs()->getHashMode() + 1 == r.path_hash_size);
  }

  int matchingBuiltin(ZenPrefs* p, const Target& t) const {
    int generic = -1;
    for (int i = 0; i < RADIO_PRESET_COUNT; ++i) {
      if (!matchesBuiltin(p, t, RADIO_PRESETS[i])) continue;
      if (RADIO_PRESETS[i].path_hash_size) return i;
      if (generic < 0) generic = i;
    }
    return generic;
  }

  const char* currentName(ZenPrefs* p, const Target& t) const {
    if (!p) return "Custom";
    int builtin = matchingBuiltin(p, t);
    if (builtin >= 0) return RADIO_PRESETS[builtin].name;
    for (const auto& u : p->user_radio_presets)
      if (u.name[0] && matches(t, u.freq, u.bw, u.sf, u.cr)) return u.name;
    return "Custom";
  }

  int builtinStart() const { return int(page) * PAGE_SIZE; }
  int builtinCount() const {
    int count = RADIO_PRESET_COUNT - builtinStart();
    return count < PAGE_SIZE ? count : PAGE_SIZE;
  }
  int pageCount() const { return (RADIO_PRESET_COUNT + PAGE_SIZE - 1) / PAGE_SIZE; }

  int currentListIndex(ZenPrefs* p, const Target& t) const {
    if (!p) return -1;
    int builtin = matchingBuiltin(p, t);
    if (builtin >= builtinStart() && builtin < builtinStart() + builtinCount())
      return 1 + builtin - builtinStart();
    int row = 1 + builtinCount();
    for (const auto& u : p->user_radio_presets) {
      if (!u.name[0]) continue;
      if (matches(t, u.freq, u.bw, u.sf, u.cr)) return row;
      ++row;
    }
    return -1;
  }

  // Save the target's params as a named user preset: overwrite a slot with the
  // same name if one exists, else the first empty slot, else slot 0 (oldest).
  // Returns true if anything was written (caller alerts + marks dirty).
  bool save(ZenPrefs* p, const char* name, const Target& t) {
    if (!p || !name || !name[0]) return false;
    int slot = -1;
    for (int i = 0; i < ZenPrefs::USER_RADIO_PRESET_MAX; i++)
      if (strcmp(p->user_radio_presets[i].name, name) == 0) { slot = i; break; }
    if (slot < 0)
      for (int i = 0; i < ZenPrefs::USER_RADIO_PRESET_MAX; i++)
        if (!p->user_radio_presets[i].name[0]) { slot = i; break; }
    if (slot < 0) slot = 0;
    ZenPrefs::UserRadioPreset& u = p->user_radio_presets[slot];
    strncpy(u.name, name, sizeof(u.name) - 1);
    u.name[sizeof(u.name) - 1] = '\0';
    u.freq = *t.freq; u.bw = *t.bw; u.sf = *t.sf; u.cr = *t.cr;
    return true;
  }

  void buildPage(ZenPrefs* p, const Target& t, const char* title, bool navigation = false) {
    menu.begin(title, 6);
    menu.addItem("+ Save current...");
    for (int i = builtinStart(); i < builtinStart() + builtinCount(); ++i)
      menu.addItem(RADIO_PRESETS[i].name);
    user_count = 0;
    if (p) for (int i = 0; i < ZenPrefs::USER_RADIO_PRESET_MAX; ++i) {
      if (!p->user_radio_presets[i].name[0]) continue;
      menu.addItem(p->user_radio_presets[i].name);
      user_slot[user_count++] = uint8_t(i);
    }
    if (user_count) menu.addItem("- Delete preset...");
    if (page) menu.addItem("< Previous presets");
    if (page + 1 < pageCount()) menu.addItem("Next presets >");
    int selected = currentListIndex(p, t);
    menu.setSelected(selected >= 0 ? selected : navigation ? 1 : 0);
  }

  void open(ZenPrefs* p, const Target& t, const char* title) {
    saving = deleting = false;
    confirm_slot = -1;
    int builtin = matchingBuiltin(p, t);
    page = builtin >= 0 ? uint8_t(builtin / PAGE_SIZE) : 0;
    buildPage(p, t, title);
  }

  // Reuses `menu` for a second-level list of just the saved user presets (their
  // slot mapping in user_slot is still the one open() built, since this is only
  // reached from within that same popup).
  void openDelete(ZenPrefs* p) {
    menu.begin("Delete Preset", 6);
    for (int i = 0; i < user_count; i++)
      menu.addItem(p->user_radio_presets[user_slot[i]].name);
    deleting = true;
  }

  void openConfirm(uint8_t slot) {
    menu.beginConfirm("Delete preset?", "Delete");
    confirm_slot = slot;
  }

  // Handle the index the popup reports as SELECTED. Mutates target fields on a
  // built-in/user pick; deletes a slot in the delete sub-list. See Result.
  Result onSelected(int idx, ZenPrefs* p, const Target& t) {
    if (!p || idx < 0 || idx >= menu.count()) return NONE;
    if (confirm_slot >= 0) {
      Result r = NONE;
      if (idx == 0) {
        p->user_radio_presets[confirm_slot].name[0] = '\0';
        r = DELETED;
      }
      confirm_slot = -1;
      return r;
    }
    if (deleting) {
      if (idx >= 0 && idx < user_count) openConfirm(user_slot[idx]);
      deleting = false;
      return NONE;
    }
    const int builtin_base = 1;
    const int user_base    = builtin_base + builtinCount();
    const int delete_idx   = user_base + user_count;
    if (idx == 0) {
      saving = true;
      return START_SAVE;
    }
    if (idx >= builtin_base && idx < user_base) {
      const RadioPreset& r = RADIO_PRESETS[builtinStart() + idx - builtin_base];
      *t.freq = r.freq; *t.bw = r.bw; *t.sf = r.sf; *t.cr = r.cr;
      if (r.path_hash_size) p->getRadioPrefs()->setHashMode(r.path_hash_size - 1);
      return APPLIED;
    }
    if (idx >= user_base && idx < delete_idx) {
      const ZenPrefs::UserRadioPreset& u = p->user_radio_presets[user_slot[idx - user_base]];
      *t.freq = u.freq; *t.bw = u.bw; *t.sf = u.sf; *t.cr = u.cr;
      return APPLIED;
    }
    if (user_count > 0 && idx == delete_idx) {
      openDelete(p);
      return NONE;
    }
    int navigation = delete_idx + (user_count > 0 ? 1 : 0);
    if (page) {
      if (idx == navigation) {
        --page;
        const char* title = menu._title;
        buildPage(p, t, title, true);
        return NONE;
      }
      ++navigation;
    }
    if (page + 1 < pageCount() && idx == navigation) {
      ++page;
      const char* title = menu._title;
      buildPage(p, t, title, true);
    }
    return NONE;
  }
};
