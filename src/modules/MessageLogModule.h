#pragma once

#include "ProtobufModule.h"
#include "Observer.h"
#include "concurrency/OSThread.h"
#include "meshtastic/admin.pb.h"
#include "FSCommon.h"
#include <vector>
#include <cstdint>

// Maximum number of log entries to keep in memory before flushing to flash
#define MESSAGE_LOG_BUFFER_SIZE 50
// Maximum size of a single log file in bytes (1MB)
#define MESSAGE_LOG_MAX_FILE_SIZE (1024 * 1024)
// Maximum number of log files to keep
#define MESSAGE_LOG_MAX_FILES 10
// Base filename for log files
#define MESSAGE_LOG_BASE_FILENAME "/logs/msg_log_"

// Experiment phase configuration
struct ExperimentPhase {
    uint32_t duration_seconds;
    uint32_t send_interval_seconds;
};

static constexpr ExperimentPhase EXPERIMENT_PHASES[] = {
    {4 * 60, 60},  // Phase 1: 4 minutes, 60s interval
    {3 * 60, 30},  // Phase 2: 3 minutes, 30s interval
    {2 * 60, 20}   // Phase 3: 2 minutes, 20s interval
};

static constexpr size_t EXPERIMENT_PHASE_COUNT = sizeof(EXPERIMENT_PHASES) / sizeof(EXPERIMENT_PHASES[0]);


/**
 * Represents a single message log entry
 */
typedef struct {
    uint32_t timestamp;          // Unix timestamp when message was logged
    uint32_t from;              // Source node ID
    uint32_t to;                // Destination node ID (0 for broadcast)
    uint32_t id;                // Packet ID
    uint8_t channel;            // Channel index
    uint8_t hop_limit;          // Hop limit
    uint8_t hop_start;          // Initial hop limit
    bool is_sent;               // true for sent messages, false for received
    bool want_ack;              // Whether ACK was requested
    meshtastic_PortNum portnum; // Port number (message type)
    uint8_t payload_size;       // Size of payload in bytes
    uint8_t payload[256];       // Message payload (truncated if larger)
    int32_t rx_snr;             // Signal-to-noise ratio (for received messages, -999 for sent messages)
    int32_t rx_rssi;            // Signal strength (for received messages, -999 for sent messages)
    uint32_t packet_size;       // Total packet size including header and payload
    float channel_utilization;  // Channel utilization percentage at time of message
    float tx_utilization;       // TX utilization percentage at time of message
} MessageLogEntry;

/**
 * Admin commands for message log retrieval
 */
enum MessageLogCommand {
    MESSAGE_LOG_CMD_GET_COUNT = 1,     // Get total number of logged messages
    MESSAGE_LOG_CMD_GET_ENTRIES = 2,   // Get log entries with optional filtering
    MESSAGE_LOG_CMD_CLEAR_LOGS = 3,    // Clear all log files
    MESSAGE_LOG_CMD_GET_STATS = 4      // Get logging statistics
};

/**
 * Message Log Module for comprehensive message tracking
 * 
 * This module logs all sent and received messages to flash storage with timestamps,
 * message types, and metadata. Provides retrieval interface over USB/serial.
 */
class MessageLogModule : public ProtobufModule<meshtastic_AdminMessage>, 
                        public Observable<const meshtastic_AdminMessage *>,
                        private concurrency::OSThread
{
    /// Buffer for log entries before writing to flash
    std::vector<MessageLogEntry> logBuffer;
    
    /// Current log file index
    uint32_t currentLogFileIndex = 0;
    
    /// Current log file size
    uint32_t currentLogFileSize = 0;
    
    /// Total messages logged since boot
    uint32_t totalMessagesLogged = 0;
    
    /// Statistics
    uint32_t sentMessageCount = 0;
    uint32_t receivedMessageCount = 0;
    uint32_t bellMessageCount = 0;
    
    /// File handle for current log file
    File currentLogFile;

    bool firstTime = 1;

public:
    /** Constructor */
    MessageLogModule();
    
    /** Log a sent message */
    void logSentMessage(const meshtastic_MeshPacket &mp);
    
    /** Log a received message */  
    void logReceivedMessage(const meshtastic_MeshPacket &mp, int32_t rxSnr = 0, int32_t rxRssi = 0);
    
    /** Get time since last bell character was received (from ExternalNotificationModule) */
    uint32_t getTimeSinceLastBell();
    
    /** Get timestamp of last bell reception */
    uint32_t getLastBellTime();
    
    /** Check if any bell character has been received */
    bool hasBellBeenReceived();

protected:
    /** Called to handle a particular incoming AdminMessage for log retrieval */
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_AdminMessage *p) override;
    
    /** Periodic processing */
    virtual int32_t runOnce() override;

private:
    /** Write log buffer to flash storage */
    void flushLogBuffer();
    
    /** Create a new log file */
    bool createNewLogFile();
    
    /** Close current log file */
    void closeCurrentLogFile();
    
    /** Get list of existing log files */
    std::vector<std::string> getLogFiles();
    
    /** Read log entries from file */
    std::vector<MessageLogEntry> readLogEntriesFromFile(const std::string& filename, 
                                                       uint32_t maxEntries = 0, 
                                                       uint32_t skipEntries = 0);
    
    /** Handle log retrieval command */
    meshtastic_MeshPacket* handleLogRetrievalCommand(const meshtastic_MeshPacket &req, 
                                                    MessageLogCommand cmd, 
                                                    const uint8_t* params, 
                                                    size_t paramSize);
    
    /** Create log entry from mesh packet */
    MessageLogEntry createLogEntry(const meshtastic_MeshPacket &mp, bool isSent, 
                                  int32_t rxSnr = 0, int32_t rxRssi = 0);
    
    /** Convert port number to human-readable message type */
    const char* getMessageTypeString(meshtastic_PortNum portnum);
    
    /** Check if message contains bell character */
    bool containsBellCharacter(const meshtastic_MeshPacket &mp);
    
    /** Initialize log directory */
    void initLogDirectory();
    
    /** Clean up old log files if too many exist */
    void cleanupOldLogFiles();

    /** Delete all log files */
    void deleteAllLogFiles();
    
    /** Get total size of all log files */
    uint32_t getTotalLogSize();

    /** prints all log entries */
    void printAllLogEntries();

    /** Check if message is important (e.g., POSITION, NODEINFO, TELEMETRY) */
    bool isImportantMessage(const meshtastic_MeshPacket &mp);

    /** Start Phases of the Expirement */
    void startExperimentPhases();
};

extern MessageLogModule *messageLogModule;
