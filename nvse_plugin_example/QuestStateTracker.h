#pragma once
#ifndef QUEST_STATE_TRACKER_H
#define QUEST_STATE_TRACKER_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <sstream>

#include "nvse/GameForms.h"
#include "nvse/GameObjects.h"
#include "nvse/PluginAPI.h"
#include "Message.h"

// Forward declarations
class NetworkManager;

/**
 * @brief Structure to represent a quest objective state
 */
struct ObjectiveState {
    UInt32 objectiveId;
    bool displayed;
    bool completed;
    std::string displayText;

    ObjectiveState() : objectiveId(0), displayed(false), completed(false), displayText("") {}

    ObjectiveState(UInt32 id, bool isDisplayed, bool isCompleted, const std::string& text)
        : objectiveId(id), displayed(isDisplayed), completed(isCompleted), displayText(text) {}

    // Equality operator for comparing states
    bool operator==(const ObjectiveState& other) const {
        return objectiveId == other.objectiveId &&
               displayed == other.displayed &&
               completed == other.completed;
    }

    // Inequality operator
    bool operator!=(const ObjectiveState& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Structure to represent a quest state
 */
struct QuestState {
    UInt32 questId;
    std::string questName;
    bool active;
    bool completed;
    bool failed;
    std::unordered_map<UInt32, ObjectiveState> objectives;

    QuestState() : questId(0), questName(""), active(false), completed(false), failed(false) {}

    QuestState(UInt32 id, const std::string& name, bool isActive, bool isCompleted, bool isFailed)
        : questId(id), questName(name), active(isActive), completed(isCompleted), failed(isFailed) {}

    // Equality operator for comparing states
    bool operator==(const QuestState& other) const {
        return questId == other.questId &&
               active == other.active &&
               completed == other.completed &&
               failed == other.failed &&
               objectives == other.objectives;
    }

    // Inequality operator
    bool operator!=(const QuestState& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Class for tracking quest state changes and sending updates to the server
 */
class QuestStateTracker {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return Reference to the QuestStateTracker instance
     */
    static QuestStateTracker& GetInstance();

    /**
     * @brief Initialize the quest state tracker
     *
     * @param consoleInterface Pointer to the NVSE console interface
     * @return true if initialization was successful, false otherwise
     */
    bool Initialize(NVSEConsoleInterface* consoleInterface);

    /**
     * @brief Update quest states from the game
     *
     * This method should be called regularly to check for quest state changes
     * and send updates to the server if necessary.
     *
     * @param questObjectiveList The list of quest objectives from the game
     * @return true if any updates were sent, false otherwise
     */
    bool UpdateQuestStates(tList<BGSQuestObjective> questObjectiveList);

    /**
     * @brief Process a quest state message from the server
     *
     * @param message The message to process
     * @return true if the message was processed successfully, false otherwise
     */
    bool ProcessQuestStateMessage(const Message& message);

    /**
     * @brief Get the current state of a quest
     *
     * @param questId The quest ID
     * @return The quest state, or nullptr if not found
     */
    const QuestState* GetQuestState(UInt32 questId) const;

    /**
     * @brief Get all current quest states
     *
     * @return Map of quest IDs to quest states
     */
    const std::unordered_map<UInt32, QuestState>& GetAllQuestStates() const;

    /**
     * @brief Reset all quest states
     *
     * This should be called when starting a new game or loading a save.
     */
    void Reset();

    /**
     * @brief Set the network manager
     *
     * @param networkManager Pointer to the network manager
     */
    void SetNetworkManager(NetworkManager* networkManager);

    /**
     * @brief Execute a console command to update quest state in the game
     *
     * @param command The console command to execute
     * @return true if the command was executed successfully, false otherwise
     */
    bool ExecuteConsoleCommand(const std::string& command);

private:
    // Private constructor for singleton
    QuestStateTracker();

    // Deleted copy constructor and assignment operator
    QuestStateTracker(const QuestStateTracker&) = delete;
    QuestStateTracker& operator=(const QuestStateTracker&) = delete;

    // Console interface for executing commands
    NVSEConsoleInterface* m_consoleInterface;

    // Network manager for sending messages
    NetworkManager* m_networkManager;

    // Current quest states
    std::unordered_map<UInt32, QuestState> m_questStates;

    // Previous quest states for change detection
    std::unordered_map<UInt32, QuestState> m_previousQuestStates;

    // Initialization state
    bool m_initialized;

    // Helper methods
    bool DetectQuestChanges(const std::unordered_map<UInt32, QuestState>& currentStates);
    void SendQuestUpdate(const QuestState& questState);
    void SendObjectiveUpdate(const QuestState& questState, const ObjectiveState& objectiveState);
    std::string QuestIdToHexString(UInt32 questId) const;
    QuestState CreateQuestStateFromGameQuest(TESQuest* quest);
    ObjectiveState CreateObjectiveStateFromGameObjective(BGSQuestObjective* objective);

    // Quest state manipulation methods
    bool StartQuest(const std::string& questId);
    bool SetQuestStage(const std::string& questId, UInt32 stage);
    bool CompleteQuest(const std::string& questId);
    bool FailQuest(const std::string& questId);
};

#endif // QUEST_STATE_TRACKER_H
