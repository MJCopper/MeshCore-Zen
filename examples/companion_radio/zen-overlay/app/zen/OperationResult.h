#pragma once

#include <stdint.h>
#include <stdio.h>

namespace zen {

enum class Operation : uint8_t {
  UNKNOWN, FAVOURITES, CONTACTS, PING, MESSAGE, CHANNEL, ROOM, ROOM_LOGIN,
  SENSOR_LOGIN, TELEMETRY, ADMIN_LOGIN, ADMIN, ADVERT, RINGTONE,
  SETTINGS_IMPORT, CHILD_PIN, CHILD_MODE, FLOOD_SCOPE, STORAGE, GPS,
  BLUETOOTH, RADIO, SCREEN, CARDKB, POWER, BATTERY, LOCATION, TIME_SYNC
};

enum class OperationOutcome : uint8_t {
  PENDING, SUCCESS, WARNING, TIMEOUT, REJECTED, INVALID_RESPONSE,
  TRANSPORT_FAILURE, PERMISSION_DENIED, BUSY, NOT_FOUND,
  VERIFICATION_FAILED, CANCELLED, FAULT, UNKNOWN
};

enum class OperationReason : uint8_t {
  NONE, LIST_FULL, SEND_FAILED, NOT_QUEUED, NO_REPLY, RETRIES_EXHAUSTED,
  NO_ACK, NO_RELAY, LOGIN_REJECTED, NO_ACL_REPLY, NO_PASSWORD_REPLY,
  NODE_UNAVAILABLE, REQUEST_ACTIVE, LOGIN_BUSY, NOT_ADMIN, UNSUPPORTED,
  NOT_AUTHORIZED, NO_COMMAND_REPLY, RETRY_SEND_FAILED,
  DELETE_FAILED, SAVE_FAILED, PREFS_STORE_LOCKED, PREFS_TEMP_WRITE_FAILED,
  PREFS_TEMP_VERIFY_FAILED, PREFS_RENAME_FAILED, PREFS_BACKUP_FAILED,
  PREFS_PROMOTION_FAILED, PREFS_VERIFY_FAILED, DUPLICATE, REQUIRED, INVALID_INPUT,
  INVALID_SECRET, INVALID_SETTINGS_REPLY, CHANGE_REJECTED, VERIFY_MISMATCH,
  VERIFY_TIMEOUT, UNEXPECTED_REPLY, COMMAND_REJECTED, RESULT_UNKNOWN,
  PREFS_INVALID, PREFS_MISSING, PIN_MISMATCH, PARENT_ONLY,
  CONTACT_NOT_ALLOWED, CONTACT_NOT_FOUND, FAVOURITE_REQUIRED,
  BLUETOOTH_UNAVAILABLE, EMERGENCY_REQUIRED, APPLY_FAILED,
  POWER_APPLY_FAILED, GPS_RETRIES_ENDED, UNSUPPORTED_PATH, LOW_SPACE,
  STATUS_UNAVAILABLE, DISCONNECTED, QUEUE_FULL, CHANNEL_BUSY_TIMEOUT,
  RECEIVE_START_FAILED, NO_GPS_FIX, SETTING_UNAVAILABLE, INCORRECT_PIN,
  INVALID_ENTRY_REMOVED, ROOMS_UNRESTRICTED, LOW_BATTERY, LOW_POWER_MODE,
  WATCHDOG_RESET, CPU_LOCKUP
};

enum OperationResultFlag : uint8_t {
  RESULT_BACKGROUND = 1 << 0,
  RESULT_SCREEN_WAKE = 1 << 1,
  RESULT_POPUP_SUCCESS = 1 << 2,
  RESULT_LOG_SUCCESS = 1 << 3
};

struct OperationContext {
  uint8_t key[4] = {};
  uint8_t attempt = 0;
  uint8_t route = 0; // 0 none, 1 direct, 2 path, 3 flood
  int16_t value = 0;
};

struct OperationResult {
  Operation operation = Operation::UNKNOWN;
  OperationOutcome outcome = OperationOutcome::UNKNOWN;
  OperationReason reason = OperationReason::NONE;
  OperationContext context;
  uint8_t flags = RESULT_SCREEN_WAKE;

  static OperationResult make(Operation operation, OperationOutcome outcome,
                              OperationReason reason = OperationReason::NONE,
                              uint8_t flags = RESULT_SCREEN_WAKE) {
    OperationResult result;
    result.operation = operation;
    result.outcome = outcome;
    result.reason = reason;
    result.flags = flags;
    return result;
  }
};

struct OperationResultCatalog {
  static const char* operationName(Operation operation) {
    switch (operation) {
      case Operation::FAVOURITES: return "Favourites";
      case Operation::CONTACTS: return "Contacts";
      case Operation::PING: return "Ping";
      case Operation::MESSAGE: return "Message";
      case Operation::CHANNEL: return "Channel";
      case Operation::ROOM: return "Room";
      case Operation::ROOM_LOGIN: return "Room login";
      case Operation::SENSOR_LOGIN: return "Sensor login";
      case Operation::TELEMETRY: return "Telemetry";
      case Operation::ADMIN_LOGIN: return "Admin login";
      case Operation::ADMIN: return "Admin";
      case Operation::ADVERT: return "Advert";
      case Operation::RINGTONE: return "Ringtone";
      case Operation::SETTINGS_IMPORT: return "MeshCore import";
      case Operation::CHILD_PIN: return "Child PIN";
      case Operation::CHILD_MODE: return "Child Mode";
      case Operation::FLOOD_SCOPE: return "Flood scope";
      case Operation::STORAGE: return "Storage";
      case Operation::GPS: return "GPS";
      case Operation::BLUETOOTH: return "Bluetooth";
      case Operation::RADIO: return "Radio";
      case Operation::SCREEN: return "Display";
      case Operation::CARDKB: return "CardKB";
      case Operation::POWER: return "Power";
      case Operation::BATTERY: return "Battery";
      case Operation::LOCATION: return "Location";
      case Operation::TIME_SYNC: return "Time sync";
      default: return "Operation";
    }
  }

  static const char* reasonName(OperationReason reason) {
    switch (reason) {
      case OperationReason::LIST_FULL: return "List full";
      case OperationReason::SEND_FAILED: return "Send failed";
      case OperationReason::NOT_QUEUED: return "Not queued";
      case OperationReason::NO_REPLY: return "No reply";
      case OperationReason::RETRIES_EXHAUSTED: return "No reply after retries";
      case OperationReason::NO_ACK: return "No ACK after retries";
      case OperationReason::NO_RELAY: return "No relay heard";
      case OperationReason::LOGIN_REJECTED: return "Login rejected";
      case OperationReason::NO_ACL_REPLY: return "No ACL reply";
      case OperationReason::NO_PASSWORD_REPLY: return "No password reply";
      case OperationReason::NODE_UNAVAILABLE: return "Node unavailable";
      case OperationReason::REQUEST_ACTIVE: return "Another request active";
      case OperationReason::LOGIN_BUSY: return "Login busy";
      case OperationReason::NOT_ADMIN: return "Not Admin";
      case OperationReason::UNSUPPORTED: return "Not supported";
      case OperationReason::NOT_AUTHORIZED: return "Not authorized";
      case OperationReason::NO_COMMAND_REPLY: return "No command reply";
      case OperationReason::RETRY_SEND_FAILED: return "Retry send failed";
      case OperationReason::DELETE_FAILED: return "Delete failed";
      case OperationReason::SAVE_FAILED: return "Save failed";
      case OperationReason::PREFS_STORE_LOCKED: return "Prefs store locked";
      case OperationReason::PREFS_TEMP_WRITE_FAILED: return "Prefs temp write failed";
      case OperationReason::PREFS_TEMP_VERIFY_FAILED: return "Prefs temp verify failed";
      case OperationReason::PREFS_RENAME_FAILED: return "Prefs rename failed";
      case OperationReason::PREFS_BACKUP_FAILED: return "Prefs backup failed";
      case OperationReason::PREFS_PROMOTION_FAILED: return "Prefs promotion failed";
      case OperationReason::PREFS_VERIFY_FAILED: return "Prefs verify failed";
      case OperationReason::DUPLICATE: return "Already added";
      case OperationReason::REQUIRED: return "Required";
      case OperationReason::INVALID_INPUT: return "Invalid input";
      case OperationReason::INVALID_SECRET: return "Invalid secret";
      case OperationReason::INVALID_SETTINGS_REPLY: return "Invalid setting reply";
      case OperationReason::CHANGE_REJECTED: return "Change rejected";
      case OperationReason::VERIFY_MISMATCH: return "Verify mismatch";
      case OperationReason::VERIFY_TIMEOUT: return "Verify timeout";
      case OperationReason::UNEXPECTED_REPLY: return "Unexpected reply";
      case OperationReason::COMMAND_REJECTED: return "Command rejected";
      case OperationReason::RESULT_UNKNOWN: return "Result unknown";
      case OperationReason::PREFS_INVALID: return "Invalid or unavailable prefs";
      case OperationReason::PREFS_MISSING: return "Preferences file missing";
      case OperationReason::PIN_MISMATCH: return "PINs did not match";
      case OperationReason::PARENT_ONLY: return "Parent only";
      case OperationReason::CONTACT_NOT_ALLOWED: return "Contact not allowed";
      case OperationReason::CONTACT_NOT_FOUND: return "Contact not found";
      case OperationReason::FAVOURITE_REQUIRED: return "Favourite contact first";
      case OperationReason::BLUETOOTH_UNAVAILABLE: return "Bluetooth unavailable";
      case OperationReason::EMERGENCY_REQUIRED: return "Enable Emergency";
      case OperationReason::APPLY_FAILED: return "Apply failed";
      case OperationReason::POWER_APPLY_FAILED: return "Power apply failed";
      case OperationReason::GPS_RETRIES_ENDED: return "GPS retries ended";
      case OperationReason::UNSUPPORTED_PATH: return "Unsupported path";
      case OperationReason::LOW_SPACE: return "Low free space";
      case OperationReason::STATUS_UNAVAILABLE: return "Status unavailable";
      case OperationReason::DISCONNECTED: return "Disconnected";
      case OperationReason::QUEUE_FULL: return "Queue full";
      case OperationReason::CHANNEL_BUSY_TIMEOUT: return "Channel busy timeout";
      case OperationReason::RECEIVE_START_FAILED: return "Receive start failed";
      case OperationReason::NO_GPS_FIX: return "No GPS fix";
      case OperationReason::SETTING_UNAVAILABLE: return "Setting unavailable";
      case OperationReason::INCORRECT_PIN: return "Incorrect PIN";
      case OperationReason::INVALID_ENTRY_REMOVED: return "Invalid entry removed";
      case OperationReason::ROOMS_UNRESTRICTED: return "Rooms unrestricted";
      case OperationReason::LOW_BATTERY: return "Low battery";
      case OperationReason::LOW_POWER_MODE: return "Low Power Mode";
      case OperationReason::WATCHDOG_RESET: return "Watchdog reset";
      case OperationReason::CPU_LOCKUP: return "CPU lockup reset";
      default: return "";
    }
  }

  static const char* outcomeName(OperationOutcome outcome) {
    switch (outcome) {
      case OperationOutcome::SUCCESS: return "Success";
      case OperationOutcome::WARNING: return "Warning";
      case OperationOutcome::TIMEOUT: return "Timeout";
      case OperationOutcome::REJECTED: return "Rejected";
      case OperationOutcome::INVALID_RESPONSE: return "Invalid response";
      case OperationOutcome::TRANSPORT_FAILURE: return "Transport failure";
      case OperationOutcome::PERMISSION_DENIED: return "Permission denied";
      case OperationOutcome::BUSY: return "Busy";
      case OperationOutcome::NOT_FOUND: return "Not found";
      case OperationOutcome::VERIFICATION_FAILED: return "Verification failed";
      case OperationOutcome::CANCELLED: return "Cancelled";
      case OperationOutcome::FAULT: return "Fault";
      case OperationOutcome::UNKNOWN: return "Unknown";
      default: return "Pending";
    }
  }

  static const char* status(const OperationResult& result) {
    const char* reason = reasonName(result.reason);
    return reason[0] ? reason : outcomeName(result.outcome);
  }

  static bool isError(OperationOutcome outcome) {
    return outcome == OperationOutcome::TIMEOUT || outcome == OperationOutcome::REJECTED ||
        outcome == OperationOutcome::INVALID_RESPONSE ||
        outcome == OperationOutcome::TRANSPORT_FAILURE ||
        outcome == OperationOutcome::PERMISSION_DENIED ||
        outcome == OperationOutcome::NOT_FOUND ||
        outcome == OperationOutcome::VERIFICATION_FAILED ||
        outcome == OperationOutcome::FAULT;
  }
  static bool isWarning(OperationOutcome outcome) {
    return outcome == OperationOutcome::WARNING || outcome == OperationOutcome::BUSY ||
           outcome == OperationOutcome::UNKNOWN || outcome == OperationOutcome::CANCELLED;
  }
  static bool shouldLog(const OperationResult& result) {
    return isError(result.outcome) || isWarning(result.outcome) ||
           (result.outcome == OperationOutcome::SUCCESS &&
            (result.flags & RESULT_LOG_SUCCESS));
  }
  static bool shouldPopup(const OperationResult& result) {
    return isError(result.outcome) || isWarning(result.outcome) ||
           (result.outcome == OperationOutcome::SUCCESS &&
            (result.flags & RESULT_POPUP_SUCCESS));
  }
};

struct OperationStatus {
  OperationResult result;
  bool available = false;
  void set(const OperationResult& value) { result = value; available = true; }
  void clear() { available = false; }
  const char* text() const {
    return available ? OperationResultCatalog::status(result) : "";
  }
};

} // namespace zen
