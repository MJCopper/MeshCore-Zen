#pragma once

#include "OperationResult.h"
#include "RemoteNodeOperation.h"

namespace zen {

struct RemoteOperationResultAdapter {
  static OperationOutcome outcome(RemoteNodeOperation::Result result) {
    switch (result) {
      case RemoteNodeOperation::SUCCESS: return OperationOutcome::SUCCESS;
      case RemoteNodeOperation::REJECTED: return OperationOutcome::REJECTED;
      case RemoteNodeOperation::PERMISSION_DENIED: return OperationOutcome::PERMISSION_DENIED;
      case RemoteNodeOperation::MALFORMED_REPLY: return OperationOutcome::INVALID_RESPONSE;
      case RemoteNodeOperation::NODE_MISSING: return OperationOutcome::NOT_FOUND;
      case RemoteNodeOperation::BUSY: return OperationOutcome::BUSY;
      case RemoteNodeOperation::SEND_FAILED: return OperationOutcome::TRANSPORT_FAILURE;
      case RemoteNodeOperation::TIMEOUT:
      case RemoteNodeOperation::RETRIES_EXHAUSTED: return OperationOutcome::TIMEOUT;
      case RemoteNodeOperation::CANCELLED: return OperationOutcome::CANCELLED;
      case RemoteNodeOperation::RESULT_UNKNOWN: return OperationOutcome::UNKNOWN;
      default: return OperationOutcome::PENDING;
    }
  }

  static OperationResult convert(Operation operation,
                                 const RemoteNodeOperation& remote,
                                 RemoteNodeOperation::Result result,
                                 uint8_t flags = RESULT_BACKGROUND) {
    OperationReason reason = OperationReason::NONE;
    if (result == RemoteNodeOperation::SEND_FAILED) reason = OperationReason::SEND_FAILED;
    else if (result == RemoteNodeOperation::RETRIES_EXHAUSTED)
      reason = OperationReason::RETRIES_EXHAUSTED;
    else if (result == RemoteNodeOperation::MALFORMED_REPLY)
      reason = OperationReason::UNEXPECTED_REPLY;
    OperationResult converted = OperationResult::make(operation, outcome(result), reason, flags);
    converted.context.attempt = remote.attempt();
    converted.context.route = remote.route() == RemoteNodeOperation::PATH ? 2 : 3;
    memcpy(converted.context.key, remote.key(), sizeof(converted.context.key));
    return converted;
  }
};

} // namespace zen
