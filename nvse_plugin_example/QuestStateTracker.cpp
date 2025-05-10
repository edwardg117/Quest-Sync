#include "QuestStateTracker.h"
#include "NetworkManager.h"
#include "nvse/PluginAPI.h"
#include <sstream>
#include <iomanip>
#include <map>

// Singleton instance
QuestStateTracker& QuestStateTracker::GetInstance() {
    static QuestStateTracker instance;
    return instance;
}

// Constructor
QuestStateTracker::QuestStateTracker()
    : m_consoleInterface(nullptr),
      m_networkManager(nullptr),
      m_initialized(false) {
}

// Initialize the quest state tracker
bool QuestStateTracker::Initialize(NVSEConsoleInterface* consoleInterface) {
    if (m_initialized) {
        return true;
    }

    if (!consoleInterface) {
        _MESSAGE("QuestStateTracker: Console interface is null");
        return false;
    }

    m_consoleInterface = consoleInterface;
    m_initialized = true;
    _MESSAGE("QuestStateTracker initialized successfully");
    return true;
}

// Set the network manager
void QuestStateTracker::SetNetworkManager(NetworkManager* networkManager) {
    m_networkManager = networkManager;
}

// Execute a console command
bool QuestStateTracker::ExecuteConsoleCommand(const std::string& command) {
    if (!m_consoleInterface) {
        _MESSAGE("QuestStateTracker: Cannot execute command, console interface is null");
        return false;
    }

    _MESSAGE("QuestStateTracker: Executing console command: %s", command.c_str());
    m_consoleInterface->RunScriptLine(command.c_str(), nullptr);
    return true;
}

// Update quest states from the game
bool QuestStateTracker::UpdateQuestStates(tList<BGSQuestObjective> questObjectiveList) {
    // Only log occasionally to avoid filling the log file
    static int logCounter = 0;
    bool shouldLog = (logCounter++ % 1000 == 0);

    if (shouldLog) {
        _MESSAGE("QuestStateTracker::UpdateQuestStates called with %d objectives", questObjectiveList.Count());
    }

    if (!m_initialized) {
        _MESSAGE("QuestStateTracker: Not initialized");
        return false;
    }

    // Add logging for network manager state only when we're logging other things
    if (shouldLog) {
        _MESSAGE("QuestStateTracker: Network manager is %s", m_networkManager ? "valid" : "NULL");
    }

    // Store previous states for change detection
    m_previousQuestStates = m_questStates;

    // Clear current states to rebuild them
    m_questStates.clear();

    // Process all objectives in the list
    auto iterator = questObjectiveList.Head();
    while (iterator) {
        BGSQuestObjective* objective = iterator->data;
        if (objective && objective->quest) {
            UInt32 questId = objective->quest->refID;

            // Create or update quest state
            if (m_questStates.find(questId) == m_questStates.end()) {
                m_questStates[questId] = CreateQuestStateFromGameQuest(objective->quest);
            }

            // Add objective to quest state
            ObjectiveState objState = CreateObjectiveStateFromGameObjective(objective);
            m_questStates[questId].objectives[objective->objectiveId] = objState;
        }

        iterator = iterator->next;
    }

    // Detect changes and send updates
    return DetectQuestChanges(m_questStates);
}

// Helper function to parse key-value pairs from a string
std::map<std::string, std::string> ParseKeyValuePairs(const std::string& str) {
    std::map<std::string, std::string> result;

    size_t pos = 0;
    while (pos < str.length()) {
        // Find key start (skip whitespace)
        while (pos < str.length() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r')) {
            pos++;
        }

        // Find key end
        size_t keyStart = pos;
        while (pos < str.length() && str[pos] != '=') {
            pos++;
        }

        if (pos >= str.length()) break;

        std::string key = str.substr(keyStart, pos - keyStart);
        pos++; // Skip '='

        // Find value start
        size_t valueStart = pos;

        // Check if value is quoted
        bool quoted = false;
        if (pos < str.length() && str[pos] == '"') {
            quoted = true;
            valueStart++;
            pos++;
        }

        // Find value end
        if (quoted) {
            while (pos < str.length() && str[pos] != '"') {
                pos++;
            }
        } else {
            while (pos < str.length() && str[pos] != ';') {
                pos++;
            }
        }

        std::string value = str.substr(valueStart, pos - valueStart);

        // Skip closing quote or semicolon
        pos++;

        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t\n\r"));
        key.erase(key.find_last_not_of(" \t\n\r") + 1);
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        result[key] = value;
    }

    return result;
}

// Process a quest state message from the server
bool QuestStateTracker::ProcessQuestStateMessage(const Message& message) {
    if (!m_initialized) {
        _MESSAGE("QuestStateTracker: Not initialized");
        return false;
    }

    // Get message type
    MessageType type = message.GetType();

    // Get payload as string
    std::string payload = message.GetPayloadAsString();

    // Parse payload as key-value pairs
    std::map<std::string, std::string> data = ParseKeyValuePairs(payload);

    // Process message based on type
    switch (type) {
        case MessageType::UPDATE_QUEST: {
            // Extract quest ID and stage
            auto questIdIt = data.find("questId");
            auto stageIt = data.find("stage");

            if (questIdIt != data.end() && stageIt != data.end()) {
                std::string questIdHex = questIdIt->second;
                UInt32 stage = static_cast<UInt32>(std::stoul(stageIt->second));

                // Update quest stage in game
                return SetQuestStage(questIdHex, stage);
            }
            break;
        }

        case MessageType::COMPLETE_QUEST: {
            // Extract quest ID
            auto questIdIt = data.find("questId");

            if (questIdIt != data.end()) {
                std::string questIdHex = questIdIt->second;

                // Complete quest in game
                return CompleteQuest(questIdHex);
            }
            break;
        }

        case MessageType::FAIL_QUEST: {
            // Extract quest ID
            auto questIdIt = data.find("questId");

            if (questIdIt != data.end()) {
                std::string questIdHex = questIdIt->second;

                // Fail quest in game
                return FailQuest(questIdHex);
            }
            break;
        }

        case MessageType::START_QUEST: {
            // Extract quest ID
            auto questIdIt = data.find("questId");

            if (questIdIt != data.end()) {
                std::string questIdHex = questIdIt->second;

                // Start quest in game
                return StartQuest(questIdHex);
            }
            break;
        }

        case MessageType::COMPLETE_OBJECTIVE: {
            // Extract quest ID and objective ID
            auto questIdIt = data.find("questId");
            auto objectiveIdIt = data.find("objectiveId");

            if (questIdIt != data.end() && objectiveIdIt != data.end()) {
                std::string questIdHex = questIdIt->second;
                UInt32 objectiveId = static_cast<UInt32>(std::stoul(objectiveIdIt->second));

                // Complete objective in game
                return SetQuestStage(questIdHex, objectiveId);
            }
            break;
        }

        default:
            _MESSAGE("QuestStateTracker: Unhandled message type %d", static_cast<int>(type));
            break;
    }

    return false;
}

// Get the current state of a quest
const QuestState* QuestStateTracker::GetQuestState(UInt32 questId) const {
    auto it = m_questStates.find(questId);
    if (it != m_questStates.end()) {
        return &(it->second);
    }
    return nullptr;
}

// Get all current quest states
const std::unordered_map<UInt32, QuestState>& QuestStateTracker::GetAllQuestStates() const {
    return m_questStates;
}

// Reset all quest states
void QuestStateTracker::Reset() {
    m_questStates.clear();
    m_previousQuestStates.clear();
    _MESSAGE("QuestStateTracker: Reset all quest states");
}

// Detect quest changes and send updates
bool QuestStateTracker::DetectQuestChanges(const std::unordered_map<UInt32, QuestState>& currentStates) {
    // Only log occasionally or when there are changes
    static int logCounter = 0;
    bool shouldLog = (logCounter++ % 1000 == 0);

    if (shouldLog) {
        _MESSAGE("QuestStateTracker::DetectQuestChanges called with %d quest states", currentStates.size());
    }

    if (!m_networkManager) {
        if (shouldLog) {
            _MESSAGE("QuestStateTracker: Cannot send updates, network manager is null");
        }
        return false;
    }

    if (!m_networkManager->IsConnected()) {
        if (shouldLog) {
            _MESSAGE("QuestStateTracker: Cannot send updates, not connected to server");
        }
        return false;
    }

    if (shouldLog) {
        _MESSAGE("QuestStateTracker: Previous states count: %d", m_previousQuestStates.size());
    }

    bool changesSent = false;

    // Check if this is the first update (after loading a save)
    bool isFirstUpdate = m_previousQuestStates.empty() && !currentStates.empty();

    // If this is the first update, only send active quests to avoid overwhelming the server
    if (isFirstUpdate) {
        _MESSAGE("QuestStateTracker: First update after loading save, sending only active quests");

        // Add a significant delay before sending any quest updates after loading a save
        // This gives the connection time to stabilize
        _MESSAGE("QuestStateTracker: Waiting 3 seconds before sending quest updates to ensure connection stability");
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Check if we're still connected after the delay
        if (!m_networkManager || !m_networkManager->IsConnected()) {
            _MESSAGE("QuestStateTracker: Not connected after delay, cannot send updates");
            return false;
        }

        // Count active quests
        int activeQuestCount = 0;
        std::vector<const QuestState*> activeQuests;

        for (const auto& [questId, state] : currentStates) {
            // Skip blacklisted quests (tutorial quests and others that don't need syncing)
            if (questId == 0x104c1c || questId == 0x10a214 ||  // "Ain't That a Kick in the Head" and "Back in the Saddle"
                questId == 0x00013b || questId == 0x00013c) {  // Add more blacklisted quests as needed
                _MESSAGE("QuestStateTracker: Skipping blacklisted quest %s (%s)",
                         state.questName.c_str(), QuestIdToHexString(questId).c_str());
                continue;
            }

            if (state.active) {
                activeQuestCount++;
                activeQuests.push_back(&state);
            }
        }

        _MESSAGE("QuestStateTracker: Found %d active quests", activeQuestCount);

        // If we have active quests, send them in batches
        if (!activeQuests.empty()) {
            // Send active quests in smaller batches with longer delays between batches
            const int batchSize = 1; // Send only one quest at a time
            int batchCount = (activeQuestCount + batchSize - 1) / batchSize; // Ceiling division

            _MESSAGE("QuestStateTracker: Sending active quests in %d batches of up to %d quests each",
                     batchCount, batchSize);

            // Limit the number of quests we send initially to avoid overwhelming the server
            const int maxInitialQuests = 5;
            int questsToSend = (activeQuestCount > maxInitialQuests) ? maxInitialQuests : activeQuestCount;

            _MESSAGE("QuestStateTracker: Limiting initial sync to %d quests", questsToSend);

            for (int batch = 0; batch < questsToSend; batch++) {
                // Check connection before each batch
                if (!m_networkManager->IsConnected()) {
                    _MESSAGE("QuestStateTracker: Connection lost during batch sending, aborting");
                    return changesSent;
                }

                _MESSAGE("QuestStateTracker: Sending batch %d/%d (quest %d)",
                         batch + 1, questsToSend, batch + 1);

                // Send quest in this batch
                SendQuestUpdate(*activeQuests[batch]);
                changesSent = true;

                // Process any pending messages before sending the next batch
                if (m_networkManager) {
                    // Instead of directly calling ProcessQueuedMessages, we'll call ProcessMessages
                    // which will internally call ProcessQueuedMessages
                    m_networkManager->ProcessMessages();
                }

                // Add a longer delay between batches to avoid overwhelming the server
                if (batch < questsToSend - 1) {
                    _MESSAGE("QuestStateTracker: Waiting before sending next batch");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            }

            if (activeQuestCount > maxInitialQuests) {
                _MESSAGE("QuestStateTracker: Remaining %d quests will be synced in subsequent updates",
                         activeQuestCount - maxInitialQuests);
            }
        }

        return changesSent;
    }

    // Normal update - check for new or updated quests
    for (const auto& [questId, currentState] : currentStates) {
        // Skip blacklisted quests (tutorial quests)
        if (questId == 0x104c1c || questId == 0x10a214) {  // "Ain't That a Kick in the Head" and "Back in the Saddle"
            continue;
        }

        auto prevIt = m_previousQuestStates.find(questId);

        if (prevIt == m_previousQuestStates.end()) {
            // New quest
            _MESSAGE("QuestStateTracker: New quest detected: %s (%s)",
                     currentState.questName.c_str(), QuestIdToHexString(questId).c_str());
            SendQuestUpdate(currentState);
            changesSent = true;
        } else if (currentState != prevIt->second) {
            // Quest state changed
            const QuestState& prevState = prevIt->second;

            // Check if quest flags changed
            if (currentState.active != prevState.active ||
                currentState.completed != prevState.completed ||
                currentState.failed != prevState.failed) {
                _MESSAGE("QuestStateTracker: Quest state changed for %s (%s)",
                         currentState.questName.c_str(), QuestIdToHexString(questId).c_str());
                SendQuestUpdate(currentState);
                changesSent = true;
            }

            // Check for objective changes
            for (const auto& [objId, objState] : currentState.objectives) {
                auto prevObjIt = prevState.objectives.find(objId);

                if (prevObjIt == prevState.objectives.end()) {
                    // New objective
                    _MESSAGE("QuestStateTracker: New objective %d for quest %s",
                             objId, currentState.questName.c_str());
                    SendObjectiveUpdate(currentState, objState);
                    changesSent = true;
                } else if (objState != prevObjIt->second) {
                    // Objective state changed
                    _MESSAGE("QuestStateTracker: Objective %d changed for quest %s",
                             objId, currentState.questName.c_str());
                    SendObjectiveUpdate(currentState, objState);
                    changesSent = true;
                }
            }
        }
    }

    return changesSent;
}

// Helper function to create a key-value string
std::string CreateKeyValueString(const std::map<std::string, std::string>& data) {
    std::stringstream ss;

    for (const auto& [key, value] : data) {
        // Check if value needs quotes (contains spaces or special characters)
        bool needsQuotes = value.find_first_of(" \t\n\r;=") != std::string::npos;

        ss << key << "=";
        if (needsQuotes) {
            ss << "\"" << value << "\"";
        } else {
            ss << value;
        }
        ss << ";";
    }

    return ss.str();
}

// Send quest update to server
void QuestStateTracker::SendQuestUpdate(const QuestState& questState) {
    if (!m_networkManager) {
        _MESSAGE("QuestStateTracker: Cannot send quest update, network manager is null");
        return;
    }

    // Create payload as key-value pairs
    std::map<std::string, std::string> payload;
    payload["ID"] = QuestIdToHexString(questState.questId);
    payload["Name"] = questState.questName;
    payload["Stage"] = "";
    payload["Flags"] = "000000";
    payload["active"] = questState.active ? "true" : "false";
    payload["completed"] = questState.completed ? "true" : "false";
    payload["failed"] = questState.failed ? "true" : "false";

    // Determine message type
    MessageType type;
    if (questState.completed) {
        type = MessageType::QUEST_COMPLETED;
    } else if (questState.failed) {
        type = MessageType::QUEST_FAILED;
    } else if (!questState.active) {
        type = MessageType::QUEST_INACTIVE;
    } else {
        type = MessageType::QUEST_UPDATED;
    }

    // Only log important state changes (completed, failed) or occasionally
    static int logCounter = 0;
    bool shouldLog = (questState.completed || questState.failed || logCounter++ % 10 == 0);

    if (shouldLog) {
        _MESSAGE("QuestStateTracker: Sending update for quest %s (%s) - Type: %d, Active: %s, Completed: %s, Failed: %s",
                 questState.questName.c_str(),
                 QuestIdToHexString(questState.questId).c_str(),
                 static_cast<int>(type),
                 questState.active ? "true" : "false",
                 questState.completed ? "true" : "false",
                 questState.failed ? "true" : "false");
    }

    // Send message
    std::string payloadStr = CreateKeyValueString(payload);
    m_networkManager->QueueMessage(type, payloadStr);
}

// Send objective update to server
void QuestStateTracker::SendObjectiveUpdate(const QuestState& questState, const ObjectiveState& objectiveState) {
    if (!m_networkManager) {
        _MESSAGE("QuestStateTracker: Cannot send objective update, network manager is null");
        return;
    }

    // Create payload as key-value pairs
    std::map<std::string, std::string> payload;
    payload["ID"] = QuestIdToHexString(questState.questId);
    payload["Name"] = questState.questName;
    payload["Stage"] = "";
    payload["Flags"] = "000000";
    payload["objectiveId"] = std::to_string(objectiveState.objectiveId);
    payload["displayText"] = objectiveState.displayText;
    payload["displayed"] = objectiveState.displayed ? "true" : "false";
    payload["completed"] = objectiveState.completed ? "true" : "false";

    // Determine message type
    MessageType type;
    if (objectiveState.completed) {
        type = MessageType::OBJECTIVE_COMPLETED;
    } else {
        type = MessageType::QUEST_UPDATED;
    }

    // Only log completed objectives or occasionally
    static int logCounter = 0;
    bool shouldLog = (objectiveState.completed || logCounter++ % 10 == 0);

    if (shouldLog) {
        _MESSAGE("QuestStateTracker: Sending objective update for quest %s (%s), objective %d - Type: %d, Completed: %s",
                 questState.questName.c_str(),
                 QuestIdToHexString(questState.questId).c_str(),
                 objectiveState.objectiveId,
                 static_cast<int>(type),
                 objectiveState.completed ? "true" : "false");
    }

    // Send message
    std::string payloadStr = CreateKeyValueString(payload);
    m_networkManager->QueueMessage(type, payloadStr);
}

// Convert quest ID to hex string
std::string QuestStateTracker::QuestIdToHexString(UInt32 questId) const {
    std::stringstream ss;
    ss << std::hex << questId;
    return ss.str();
}

// Create quest state from game quest
QuestState QuestStateTracker::CreateQuestStateFromGameQuest(TESQuest* quest) {
    if (!quest) {
        return QuestState();
    }

    std::string name = quest->GetFullName() ? quest->GetFullName()->name.CStr() : "<no name>";
    bool active = (quest->flags & 1) == 1;
    bool completed = (quest->flags & 2) == 2;
    bool failed = (quest->flags & 64) == 64;

    return QuestState(quest->refID, name, active, completed, failed);
}

// Create objective state from game objective
ObjectiveState QuestStateTracker::CreateObjectiveStateFromGameObjective(BGSQuestObjective* objective) {
    if (!objective) {
        return ObjectiveState();
    }

    UInt32 id = objective->objectiveId;
    bool displayed = (objective->status & 1) == 1;
    bool completed = (objective->status & 2) == 2;
    std::string text = objective->displayText.m_data ? objective->displayText.m_data : "";

    return ObjectiveState(id, displayed, completed, text);
}

// Start a quest
bool QuestStateTracker::StartQuest(const std::string& questId) {
    std::string command = "StartQuest " + questId;
    return ExecuteConsoleCommand(command);
}

// Set a quest stage
bool QuestStateTracker::SetQuestStage(const std::string& questId, UInt32 stage) {
    std::string command = "SetStage " + questId + " " + std::to_string(stage);
    return ExecuteConsoleCommand(command);
}

// Complete a quest
bool QuestStateTracker::CompleteQuest(const std::string& questId) {
    std::string command = "CompleteQuest " + questId;
    return ExecuteConsoleCommand(command);
}

// Fail a quest
bool QuestStateTracker::FailQuest(const std::string& questId) {
    std::string command = "FailQuest " + questId;
    return ExecuteConsoleCommand(command);
}


