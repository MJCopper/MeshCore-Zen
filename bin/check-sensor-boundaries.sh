#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Pin the verified upstream release commit; the fork has an unrelated release tag.
baseline=d9296435
git diff --exit-code "$baseline" -- \
  examples/simple_sensor/main.cpp \
  src/helpers/sensors/EnvironmentSensorManager.cpp

if rg -n 'PUBLIC_CHANNEL_SENSOR_BOT|hillvue|BME680AirQuality|Bme680Telemetry|SensorBotService|SensorManagementPolicy|Bme680SensorMesh|TraceRouteCandidates|TraceTimingPolicy|TraceReportDelivery|TraceRouteHistory|TraceDiagnostics' \
    examples/simple_sensor src/Dispatcher.cpp src/Dispatcher.h src/Mesh.cpp \
    src/helpers/ClientACL.cpp src/helpers/ClientACL.h \
    src/helpers/sensors/EnvironmentSensorManager.cpp; then
  echo "Custom sensor policy has leaked into baseline code" >&2
  exit 1
fi
echo "Sensor boundaries match the v1.17.1 integration contract"
