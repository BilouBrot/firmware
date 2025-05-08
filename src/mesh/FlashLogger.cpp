#include "FlashLogger.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "configuration.h"
#include "concurrency/LockGuard.h"

namespace meshtastic {

// Create a global instance
FlashLogger flashLogger;

void FlashLogger::writeLog(const char* logLevel, const char* message) 
{
    // Take firmware's SPI lock
    concurrency::LockGuard guard(spiLock);
    
    #ifdef FSCom
    // Check if file exists and is too large
    if (FSCom.exists(logFilename)) {
        File f = FSCom.open(logFilename, "r");  // Use "r" instead of FILE_O_READ
        if (f && f.size() > maxLogSize) {
            f.close();
            FSCom.remove(logFilename);
        } else if (f) {
            f.close();
        }
    }
    
    // Append to log file
    File f = FSCom.open(logFilename, "a");  // Use "a" instead of FILE_O_WRITE | FILE_O_APPEND
    if (f) {
        // Add timestamp
        f.printf("%u [%s] ", millis(), logLevel);
        f.println(message);
        f.close();
    }
    #endif
}

void FlashLogger::clear() 
{
    concurrency::LockGuard guard(spiLock);
    #ifdef FSCom
    FSCom.remove(logFilename);
    #endif
}

bool FlashLogger::readLog(char* buffer, size_t bufferSize, size_t offset) 
{
    bool success = false;
    concurrency::LockGuard guard(spiLock);
    
    #ifdef FSCom
    if (FSCom.exists(logFilename)) {
        File f = FSCom.open(logFilename, FILE_O_READ);
        if (f) {
            // Check if offset is valid
            if (offset < f.size()) {
                f.seek(offset);
                size_t bytesToRead = min(bufferSize - 1, f.size() - offset);
                size_t bytesRead = f.readBytes(buffer, bytesToRead);
                buffer[bytesRead] = '\0'; // Null-terminate the string
                success = true;
            }
            f.close();
        }
    }
    #endif
    
    return success;
}

size_t FlashLogger::getLogSize() 
{
    size_t size = 0;
    concurrency::LockGuard guard(spiLock);
    
    #ifdef FSCom
    if (FSCom.exists(logFilename)) {
        File f = FSCom.open(logFilename, FILE_O_READ);
        if (f) {
            size = f.size();
            f.close();
        }
    }
    #endif
    
    return size;
}

} // namespace meshtastic