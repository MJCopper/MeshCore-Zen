#pragma once

#include "DiagnosticLog.h"

namespace zen {

class OperationResultCoordinator {
 public:
  static const uint32_t BACKGROUND_POPUP_DEDUP_MS = 300000UL;
  struct Decision {
    bool logged = false;
    bool popup = false;
    bool repeated = false;
    DiagnosticLog::Severity severity = DiagnosticLog::INFO;
  };

  Decision publish(DiagnosticLog& log, uint32_t utc_time, uint32_t now_ms,
                   const OperationResult& result) const {
    Decision decision;
    decision.severity = DiagnosticLog::severity(result);
    const DiagnosticLog::Entry* previous = log.newest(0);
    decision.repeated = (result.flags & RESULT_BACKGROUND) && previous &&
        previous->result.operation == result.operation &&
        previous->result.outcome == result.outcome &&
        previous->result.reason == result.reason &&
        (uint32_t)(now_ms - previous->occurred_ms) < BACKGROUND_POPUP_DEDUP_MS;
    decision.logged = log.add(utc_time, now_ms, result);
    decision.popup = OperationResultCatalog::shouldPopup(result) &&
                     !decision.repeated;
    return decision;
  }
};

} // namespace zen
