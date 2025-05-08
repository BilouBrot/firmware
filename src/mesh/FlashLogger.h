#pragma once

#include "configuration.h"
#include <Arduino.h>
#include "SPILock.h"
#include "concurrency/LockGuard.h"
#ifdef FSCom
#include <FS.h>
#endif

namespace meshtastic {

class FlashLogger {
private:
    const char* logFilename = "/debug.log";
    const size_t maxLogSize = 64 * 1024; // 64KB max log size

public:
    void writeLog(const char* logLevel, const char* message);
    void clear();
    bool readLog(char* buffer, size_t bufferSize, size_t offset = 0);
    size_t getLogSize();
};

extern FlashLogger flashLogger;

} // namespace meshtastic