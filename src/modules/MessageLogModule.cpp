#include "MessageLogModule.h"
#include "Default.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Router.h"
#include "RTC.h"
#include "TypeConversions.h"
#include "configuration.h"
#include "main.h"
#include "ExternalNotificationModule.h"
#include "FSCommon.h"
#include "RadioInterface.h"
#include "airtime.h"
#include <sys/stat.h>
#include "NodeDB.h"

#ifdef FSCom
#include "SPILock.h"
#endif

MessageLogModule *messageLogModule;

// Port number strings for human-readable output
static const char* portNumToString(meshtastic_PortNum portnum) {
    switch (portnum) {
        case meshtastic_PortNum_TEXT_MESSAGE_APP: return "TEXT_MESSAGE";
        case meshtastic_PortNum_NODEINFO_APP: return "NODEINFO";
        case meshtastic_PortNum_POSITION_APP: return "POSITION";
        case meshtastic_PortNum_ADMIN_APP: return "ADMIN";
        case meshtastic_PortNum_ROUTING_APP: return "ROUTING";
        case meshtastic_PortNum_REPLY_APP: return "REPLY";
        case meshtastic_PortNum_TELEMETRY_APP: return "TELEMETRY";
        case meshtastic_PortNum_TRACEROUTE_APP: return "TRACEROUTE";
        case meshtastic_PortNum_DETECTION_SENSOR_APP: return "DETECTION_SENSOR";
        case meshtastic_PortNum_ALERT_APP: return "ALERT";
        case meshtastic_PortNum_WAYPOINT_APP: return "WAYPOINT";
        case meshtastic_PortNum_AUDIO_APP: return "AUDIO";
        case meshtastic_PortNum_PRIVATE_APP: return "PRIVATE";
        default: return "UNKNOWN";
    }
}

MessageLogModule::MessageLogModule() 
    : ProtobufModule("MessageLog", meshtastic_PortNum_ADMIN_APP, &meshtastic_AdminMessage_msg),
      concurrency::OSThread("MessageLog")
{
    logBuffer.reserve(MESSAGE_LOG_BUFFER_SIZE);
}

void MessageLogModule::logSentMessage(const meshtastic_MeshPacket &mp)
{
#ifndef FSCom
    return; // No filesystem available
#else
    if (!hasBellBeenReceived()) {
        LOG_DEBUG("No bell received, skipping logging for sent message");
        return; // Skip logging if no bell has been received
    }


    MessageLogEntry entry = createLogEntry(mp, true, -999, -999); // Use -999 to indicate N/A for sent messages
    logBuffer.push_back(entry);
    sentMessageCount++;
    totalMessagesLogged++;
    
    LOG_DEBUG("Logged sent message: from=0x%08x, to=0x%08x, id=0x%08x, port=%s", 
              mp.from, mp.to, mp.id, portNumToString(mp.decoded.portnum));
    
    // Check if message contains bell character
    if (containsBellCharacter(mp)) {
        bellMessageCount++;
        LOG_DEBUG("Sent message contains bell character");
    }
              
    // Flush if buffer is full
    if (logBuffer.size() >= MESSAGE_LOG_BUFFER_SIZE) {
        LOG_INFO("(Rec) Current log buffer size: %d, flushing to file", logBuffer.size());
        flushLogBuffer();
    }
#endif
}

void MessageLogModule::logReceivedMessage(const meshtastic_MeshPacket &mp, int32_t rxSnr, int32_t rxRssi)
{
#ifndef FSCom
    return; // No filesystem available
#else
    if (!hasBellBeenReceived()) {
        LOG_DEBUG("No bell received, skipping logging for received message");
        return; // Skip logging if no bell has been received
    }

    MessageLogEntry entry = createLogEntry(mp, false, rxSnr, rxRssi);
    logBuffer.push_back(entry);
    receivedMessageCount++;
    totalMessagesLogged++;
    
    LOG_DEBUG("Logged received message: from=0x%08x, to=0x%08x, id=0x%08x, port=%s", 
              mp.from, mp.to, mp.id, portNumToString(mp.decoded.portnum));
    
    // Check if message contains bell character
    if (containsBellCharacter(mp)) {
        bellMessageCount++;
        LOG_DEBUG("Received message contains bell character");
    }
    
    // Flush if buffer is full
    if (logBuffer.size() >= MESSAGE_LOG_BUFFER_SIZE) {
        LOG_INFO("(Sent) Current log buffer size: %d, flushing to file", logBuffer.size());
        flushLogBuffer();
    }
#endif
}

uint32_t MessageLogModule::getTimeSinceLastBell()
{
    if (externalNotificationModule) {
        return externalNotificationModule->getTimeSinceLastBell();
    }
    return UINT32_MAX; // No bell received or module not available
}

uint32_t MessageLogModule::getLastBellTime()
{
    if (externalNotificationModule) {
        return externalNotificationModule->getLastBellTime();
    }
    return 0; // No bell received or module not available
}

bool MessageLogModule::hasBellBeenReceived()
{
    if (externalNotificationModule) {
        return externalNotificationModule->hasBellBeenReceived();
    }
    return false; // No bell received or module not available
}

bool MessageLogModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_AdminMessage *r)
{
    // Only handle admin messages for message log retrieval
    if (mp.decoded.portnum != meshtastic_PortNum_ADMIN_APP) {
        return false;
    }
    
    // Check if this is a message log command
    // For now, we'll use a simple convention: admin messages with specific payload patterns
    // In a full implementation, you'd define custom protobuf messages
    
    LOG_DEBUG("Received admin message for message log");
    
    // Parse the command from the admin message
    // This is a simplified implementation - in practice you'd want proper protobuf definitions
    
    return false; // Let other handlers process this message too
}

int32_t MessageLogModule::runOnce()
{

    if (firstTime) {
        firstTime = false;
        LOG_INFO("Initializing Message Log Module");

        // Initialize log directory
        initLogDirectory();
        
        LOG_INFO("Message Log Module initialized");
    }

    if (!hasBellBeenReceived()) {
        // print old log entries if any
        printAllLogEntries();
    } else {
        startExperimentPhases();
    }

    // Periodic flush of log buffer
    if (!logBuffer.empty()) {
        LOG_INFO("Periodic flush of log buffer, size: %d", logBuffer.size());
        flushLogBuffer();
    }
    
    return 10 * 1000; // Run every 10 seconds
}

void MessageLogModule::flushLogBuffer()
{
#ifndef FSCom
    return; // No filesystem available
#else
    if (logBuffer.empty()) {
        return;
    }

    spiLock->lock();

        // Ensure we have a current log file
        if (!currentLogFile) { 
            LOG_INFO("Current log file is not valid/open, creating new log file");
            if (!createNewLogFile()) {
                LOG_ERROR("Failed to create log file for flushing");
#ifdef FSCom
                spiLock->unlock();
#endif
                return;
            }
        }

        // Write each log entry
        for (const auto& entry : logBuffer) {
            size_t written = currentLogFile.write((uint8_t*)&entry, sizeof(MessageLogEntry));
            if (written != sizeof(MessageLogEntry)) {
                LOG_ERROR("Failed to write log entry to file. Attempted: %d, Written: %d", sizeof(MessageLogEntry), written);
                closeCurrentLogFile(); 
#ifdef FSCom
                spiLock->unlock();
#endif
                return; 
            }
            currentLogFileSize += sizeof(MessageLogEntry);
        }

        // Sync to ensure data is written
        currentLogFile.flush();

        LOG_DEBUG("Flushed %d log entries to file", logBuffer.size());
        
        // Clear the buffer
        logBuffer.clear();

        // Check if we need to rotate to a new file
        if (currentLogFileSize >= MESSAGE_LOG_MAX_FILE_SIZE) {
            LOG_INFO("Current log file size exceeded limit (%u >= %u), rotating file", currentLogFileSize, MESSAGE_LOG_MAX_FILE_SIZE);
            closeCurrentLogFile();
            createNewLogFile();
        }

    spiLock->unlock();
#endif
}

bool MessageLogModule::createNewLogFile()
{
#ifndef FSCom
    return false;
#else
    closeCurrentLogFile();

    if (currentLogFileIndex == 0) {
        LOG_INFO("Starting fresh log file index");
        deleteAllLogFiles(); // Reset index if we are starting fresh
    }

    // Find next available file index
    currentLogFileIndex++;
    
    char filename[64];
    snprintf(filename, sizeof(filename), "%s%u.log", MESSAGE_LOG_BASE_FILENAME, currentLogFileIndex);

    currentLogFile = FSCom.open(filename, "w");
    if (!currentLogFile) {
        LOG_ERROR("Failed to create log file: %s", filename);
        return false;
    }

    currentLogFileSize = 0;
    LOG_INFO("Created new log file: %s", filename);
    
    // Clean up old files if we have too many
    cleanupOldLogFiles();
    
    return true;
#endif
}

void MessageLogModule::closeCurrentLogFile()
{
#ifdef FSCom
    if (currentLogFile) {
        currentLogFile.close();
        currentLogFile = File(); // Reset to empty File object
    }
#endif
}

void MessageLogModule::initLogDirectory()
{
#ifdef FSCom
    spiLock->lock();
    
    // Create logs directory if it doesn't exist
    struct stat st;
    if (stat("/logs", &st) != 0) {
        FSCom.mkdir("/logs");
        LOG_INFO("Created logs directory");
    }
    
    spiLock->unlock();
#endif
}

void MessageLogModule::cleanupOldLogFiles()
{
    // Implementation to remove old log files if we exceed MESSAGE_LOG_MAX_FILES
    // This would involve listing files and removing the oldest ones
    LOG_DEBUG("Cleanup old log files (placeholder)");
}

void MessageLogModule::deleteAllLogFiles()
{
#ifdef FSCom
    LOG_INFO("Deleting all log files");
    auto files = getLogFiles();
    for (const auto& filename : files) {
        if (FSCom.remove(filename.c_str())) {
            LOG_INFO("Deleted log file: %s", filename.c_str());
        } else {
            LOG_ERROR("Failed to delete log file: %s", filename.c_str());
        }
    }
    logBuffer.clear();
    currentLogFileIndex = 0;
    currentLogFileSize = 0;
    totalMessagesLogged = 0;
    sentMessageCount = 0;
    receivedMessageCount = 0;
    bellMessageCount = 0;
    closeCurrentLogFile();
#endif
}

std::vector<std::string> MessageLogModule::getLogFiles()
{
    std::vector<std::string> files;
    
#ifdef FSCom
    const char* logDirPath = "/logs";
    File dir = FSCom.open(logDirPath, "r");

    if (!dir) {
        LOG_ERROR("Failed to open log directory: %s", logDirPath);
        spiLock->unlock();
        return files;
    }

    if (!dir.isDirectory()) {
        LOG_ERROR("Log path is not a directory: %s", logDirPath);
        dir.close();
        spiLock->unlock();
        return files;
    }

    LOG_INFO("Scanning log directory: %s", logDirPath);
    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            const char* entryNameCStr = entry.name(); 
            if (entryNameCStr) {
                std::string entryName(entryNameCStr);
                
                const std::string prefix = "msg_log_";
                const std::string suffix = ".log";

                if (entryName.rfind(prefix, 0) == 0 &&
                    entryName.length() >= prefix.length() + suffix.length() && 
                    entryName.substr(entryName.length() - suffix.length()) == suffix) 
                {
                    std::string fullPath = std::string(logDirPath) + "/" + entryName;
                    files.push_back(fullPath);
                    LOG_DEBUG("Found log file: %s", fullPath.c_str());
                }
            }
        }
        entry.close(); 
        entry = dir.openNextFile();
    }

    dir.close(); 
#endif
    
    return files;
}

std::vector<MessageLogEntry> MessageLogModule::readLogEntriesFromFile(const std::string& filename, 
                                                                     uint32_t maxEntries, 
                                                                     uint32_t skipEntries)
{
    std::vector<MessageLogEntry> entries;
    
#ifdef FSCom
    spiLock->lock();
    
    auto file = FSCom.open(filename.c_str(), "r");
    if (file) {
        // Skip entries if requested
        file.seek(skipEntries * sizeof(MessageLogEntry));
        
        MessageLogEntry entry;
        uint32_t count = 0;
        
        while ((maxEntries == 0 || count < maxEntries) && 
               file.readBytes((char*)&entry, sizeof(MessageLogEntry)) == sizeof(MessageLogEntry)) {
            entries.push_back(entry);
            count++;
        }
        
        file.close();
    }
    
    spiLock->unlock();
#endif
    
    return entries;
}

MessageLogEntry MessageLogModule::createLogEntry(const meshtastic_MeshPacket &mp, bool isSent, 
                                                int32_t rxSnr, int32_t rxRssi)
{
    MessageLogEntry entry = {};
    
    entry.timestamp = getTimeSinceLastBell();
    entry.from = mp.from;
    entry.to = mp.to;
    entry.id = mp.id;
    entry.channel = mp.channel;
    entry.hop_limit = mp.hop_limit;
    entry.hop_start = mp.hop_start;
    entry.is_sent = isSent;
    entry.want_ack = mp.want_ack;
    entry.portnum = mp.which_payload_variant == meshtastic_MeshPacket_decoded_tag ? 
                    mp.decoded.portnum : meshtastic_PortNum_UNKNOWN_APP; // Use decoded portnum if available
    entry.rx_snr = rxSnr;
    entry.rx_rssi = rxRssi;
    
    // Calculate total packet size (header + payload)
    if (mp.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
        // For encrypted packets: encrypted size + header
        entry.packet_size = mp.encrypted.size + sizeof(PacketHeader);
    } else {
        // For decoded packets: decoded payload size + header
        entry.packet_size = mp.decoded.payload.size + sizeof(PacketHeader);
    }
    
    // Capture channel utilization at time of message
    if (airTime) {
        entry.channel_utilization = airTime->channelUtilizationPercent();
        entry.tx_utilization = airTime->utilizationTXPercent();
    } else {
        entry.channel_utilization = -1.0f; // Indicate unavailable
        entry.tx_utilization = -1.0f;      // Indicate unavailable
    }
    
    // Copy payload (truncate if too large)
    entry.payload_size = std::min((size_t)mp.decoded.payload.size, sizeof(entry.payload));
    if (entry.payload_size > 0) {
        memcpy(entry.payload, mp.decoded.payload.bytes, entry.payload_size);
    }
    
    return entry;
}

const char* MessageLogModule::getMessageTypeString(meshtastic_PortNum portnum)
{
    return portNumToString(portnum);
}

bool MessageLogModule::containsBellCharacter(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size == 0) {
        return false;
    }
    
    for (size_t i = 0; i < mp.decoded.payload.size; i++) {
        if (mp.decoded.payload.bytes[i] == 0x07) { // ASCII bell character
            return true;
        }
    }
    
    return false;
}

uint32_t MessageLogModule::getTotalLogSize()
{
    uint32_t totalSize = 0;
    
    // Add current buffer size
    totalSize += logBuffer.size() * sizeof(MessageLogEntry);
    
    // Add size of log files on disk
    auto files = getLogFiles();
    for (const auto& filename : files) {
#ifdef FSCom
        struct stat st;
        if (stat(filename.c_str(), &st) == 0) {
            totalSize += st.st_size;
        }
#endif
    }
    
    return totalSize;
}

void MessageLogModule::printAllLogEntries()
{
    #ifdef FSCom
        spiLock->lock();

        //start
        LOG_INFO("==LOGSTART==");

        // Print statistics
        LOG_INFO("ID:%u", nodeDB->getNodeNum());
        
        // Print MAC address, long name, and short name
        LOG_INFO("MAC:%02x:%02x:%02x:%02x:%02x:%02x", 
                 owner.macaddr[0], owner.macaddr[1], owner.macaddr[2], 
                 owner.macaddr[3], owner.macaddr[4], owner.macaddr[5]);
        LOG_INFO("LONG_NAME:%s", owner.long_name);
        LOG_INFO("SHORT_NAME:%s", owner.short_name);
        
        // Print buffer entries first
        for (const auto& entry : logBuffer) {
            LOG_INFO("LOG:%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%u,%.2f,%.2f",
                     entry.timestamp, entry.from, entry.to, entry.id, entry.channel,
                     entry.hop_limit, entry.hop_start, entry.is_sent ? 1 : 0,
                     entry.want_ack ? 1 : 0, entry.portnum, entry.rx_snr, entry.rx_rssi, entry.packet_size,
                     entry.channel_utilization, entry.tx_utilization);
        }
        
        // Print entries from all log files
        auto files = getLogFiles();
        for (const auto& filename : files) {
            auto file = FSCom.open(filename.c_str(), "r");
            if (file) {
                MessageLogEntry entry;
                while (file.readBytes((char*)&entry, sizeof(MessageLogEntry)) == sizeof(MessageLogEntry)) {
                    LOG_INFO("LOG:%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%u,%.2f,%.2f",
                             entry.timestamp, entry.from, entry.to, entry.id, entry.channel,
                             entry.hop_limit, entry.hop_start, entry.is_sent ? 1 : 0,
                             entry.want_ack ? 1 : 0, entry.portnum, entry.rx_snr, entry.rx_rssi, entry.packet_size,
                             entry.channel_utilization, entry.tx_utilization);
                }
                file.close();
            }
        }

        //end
        LOG_INFO("==LOGEND==");
        
        spiLock->unlock();
    #else
        LOG_INFO("LOG: Filesystem not available");
    #endif
}

bool MessageLogModule::isImportantMessage(const meshtastic_MeshPacket &mp)
{
    return (mp.decoded.portnum == meshtastic_PortNum_POSITION_APP || 
            mp.decoded.portnum == meshtastic_PortNum_NODEINFO_APP || 
            mp.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP);
}

void MessageLogModule::startExperimentPhases()
{
    if(!hasBellBeenReceived()) {
        LOG_DEBUG("No bell received, cannot start experiment phases");
        return; // Skip if no bell has been received
    }

    if (getTimeSinceLastBell() > EXPERIMENT_PHASES[0].duration_seconds * 1000 + EXPERIMENT_PHASES[1].duration_seconds * 1000) {
        // Phase 3
        LOG_INFO("Currently in Phase 3 of the experiment");
        moduleConfig.telemetry.environment_update_interval = EXPERIMENT_PHASES[2].send_interval_seconds;
    } else if (getTimeSinceLastBell() > EXPERIMENT_PHASES[0].duration_seconds * 1000) {
        // Phase 2
        LOG_INFO("Currently in Phase 2 of the experiment");
        moduleConfig.telemetry.environment_update_interval = EXPERIMENT_PHASES[1].send_interval_seconds;
    } else {
        // Phase 1
        LOG_INFO("Currently in Phase 1 of the experiment");
        moduleConfig.telemetry.environment_update_interval = EXPERIMENT_PHASES[0].send_interval_seconds;
    }

}
