#pragma once

#include <Arduino.h>

namespace boot_safety {

uint16_t registerBootAttempt();
void clearBootFailures();

void setDisplayReady(bool ready);
void setLedReady(bool ready);
void setSpeakerReady(bool ready);

[[noreturn]] void criticalFailure(const char* line1, const char* line2 = nullptr);
bool mountLittleFsWithRecovery();

}  // namespace boot_safety
