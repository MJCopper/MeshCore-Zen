#pragma once

#include "MessageDraftStore.h"

namespace zen {

// Conversation-scoped draft lifecycle. The message UI selects a target once;
// load/save/clear can no longer accidentally use a different target branch.
class MessageComposeSession {
  MessageDraftStore& _drafts;
  MessageDraftStore::Key _target = { MessageDraftStore::CHANNEL, { 0, 0, 0, 0 } };

 public:
  explicit MessageComposeSession(MessageDraftStore& drafts) : _drafts(drafts) {}

  void selectChannel(uint8_t channel) {
    _target.kind = MessageDraftStore::CHANNEL;
    _target.id[0] = channel;
    _target.id[1] = _target.id[2] = _target.id[3] = 0;
  }

  void selectContact(const uint8_t* key) {
    _target.kind = MessageDraftStore::CONTACT;
    memset(_target.id, 0, sizeof(_target.id));
    if (key) memcpy(_target.id, key, MessageDraftStore::CONTACT_PREFIX_LEN);
  }

  bool restore(char* text, size_t size, uint8_t* reply_prefix_len = nullptr) {
    return _target.kind == MessageDraftStore::CHANNEL
        ? _drafts.loadChannel(_target.id[0], text, size, reply_prefix_len)
        : _drafts.loadContact(_target.id, text, size, reply_prefix_len);
  }

  void save(const char* text, uint8_t reply_prefix_len = 0) {
    if (_target.kind == MessageDraftStore::CHANNEL)
      _drafts.saveChannel(_target.id[0], text, reply_prefix_len);
    else
      _drafts.saveContact(_target.id, text, reply_prefix_len);
  }

  void clear() {
    if (_target.kind == MessageDraftStore::CHANNEL)
      _drafts.clearChannel(_target.id[0]);
    else
      _drafts.clearContact(_target.id);
  }
};

} // namespace zen
