#pragma once

#include <helpers/IdentityStore.h>
#include <helpers/ContactInfo.h>
#include <helpers/ChannelDetails.h>
#include "NodePrefs.h"

class DataStoreHost {
public:
  virtual bool onContactLoaded(const ContactInfo& contact) =0;
  virtual bool getContactForSave(uint32_t idx, ContactInfo& contact) =0;
  virtual bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) =0;
  virtual bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) =0;
};

class DataStore {
  FILESYSTEM* _fs;
  FILESYSTEM* _fsExtra;
  mesh::RTCClock* _clock;
  IdentityStore identity_store;
  bool _last_sidecar_save_failed = false;

  void loadPrefsInt(const char *filename, NodePrefs& prefs, double& node_lat, double& node_lon);
  void loadSoloPrefs(NodePrefs& prefs);
  bool saveSoloPrefs(const NodePrefs& prefs);
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  void checkAdvBlobFile();
#endif

public:
  struct StorageStatus {
    bool available;
    bool low_space;
    uint32_t used_kb;
    uint32_t total_kb;
    uint32_t free_bytes;
  };

  DataStore(FILESYSTEM& fs, mesh::RTCClock& clock);
  DataStore(FILESYSTEM& fs, FILESYSTEM& fsExtra, mesh::RTCClock& clock);
  void begin();
  bool formatFileSystem();
  FILESYSTEM* getPrimaryFS() const { return _fs; }
  FILESYSTEM* getSecondaryFS() const { return _fsExtra; }
  bool loadMainIdentity(mesh::LocalIdentity &identity);
  bool saveMainIdentity(const mesh::LocalIdentity &identity);
  bool loadPrefs(NodePrefs& prefs, double& node_lat, double& node_lon, bool* imported = nullptr);
  bool hasMeshCorePrefs() const;
  bool importMeshCorePrefs(NodePrefs& prefs, double& node_lat, double& node_lon);
  bool savePrefs(const NodePrefs& prefs, double node_lat, double node_lon);
  bool lastSidecarSaveFailed() const { return _last_sidecar_save_failed; }
  void loadContacts(DataStoreHost* host);
  bool saveContacts(DataStoreHost* host, bool (*filter)(const ContactInfo& c) = NULL);
  // True when current or legacy channel storage exists. This lets first boot
  // seed Public without resurrecting it after a user deletes it.
  bool loadChannels(DataStoreHost* host);
  bool saveChannels(DataStoreHost* host);
  void migrateToSecondaryFS();
  uint8_t getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]);
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len);
  bool deleteBlobByKey(const uint8_t key[], int key_len);
  File openRead(const char* filename);
  File openRead(FILESYSTEM* fs, const char* filename);
  File openWrite(const char* filename);
  // Atomically replace final_path with a fully-written temp file (see
  // openWrite()). Use for small custom records that want the same crash-safety
  // as contacts/channels: write everything to a .tmp, then commit. Returns
  // false if the swap fails (the previous good file is left untouched).
  bool commitFile(const char* tmp_path, const char* final_path);
  bool removeFile(const char* filename);
  bool removeFile(FILESYSTEM* fs, const char* filename);
  uint32_t getStorageUsedKb() const;
  uint32_t getStorageTotalKb() const;
  StorageStatus getStorageStatus(bool contacts_channels) const;
  bool saveRTCTime();
  void restoreRTCTime();

private:
  FILESYSTEM* _getContactsChannelsFS() const { if (_fsExtra) return _fsExtra; return _fs;};
};
