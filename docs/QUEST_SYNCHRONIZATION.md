# Quest Synchronization System

This document describes how Quest Sync monitors, tracks, and synchronizes quest progress between multiple players in Fallout: New Vegas.

## Overview

The Quest Synchronization system is the core functionality of Quest Sync, enabling multiple players to share quest progress in real-time. The system integrates deeply with Fallout: New Vegas through the xNVSE framework to monitor quest state changes and apply updates from other players.

## Architecture

### Component Overview

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Game Engine   │    │ Quest Tracker   │    │ Network Client  │
│                 │    │                 │    │                 │
│ - Quest System  │◄──►│ - State Monitor │◄──►│ - Send Updates  │
│ - Script Engine │    │ - Change Detect │    │ - Receive Updates│
│ - Save/Load     │    │ - Validation    │    │ - Apply Changes │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### Data Flow

1. **Quest Event Detection**: Monitor game quest state changes
2. **Change Validation**: Verify quest updates are valid
3. **Message Creation**: Serialize quest data for network transmission
4. **Network Transmission**: Send updates to server
5. **Server Broadcasting**: Distribute updates to all connected clients
6. **Client Reception**: Receive quest updates from other players
7. **Game Integration**: Apply quest changes to local game state

## Quest State Tracking

### QuestStateTracker Class

The main quest tracking functionality is implemented in `QuestStateTracker.cpp`:

```cpp
class QuestStateTracker {
private:
    std::unordered_map<uint32_t, QuestState> trackedQuests;
    std::mutex questMutex;
    bool trackingEnabled;

public:
    void StartTracking();
    void StopTracking();
    void OnQuestStageChange(uint32_t questId, uint32_t newStage);
    void OnQuestComplete(uint32_t questId);
    void ApplyQuestUpdate(const QuestUpdate& update);
};
```

### Quest State Structure

```cpp
struct QuestState {
    uint32_t questId;           // Fallout NV form ID
    uint32_t currentStage;      // Current quest stage
    bool isCompleted;           // Quest completion status
    uint64_t lastUpdate;        // Timestamp of last change
    std::vector<ObjectiveState> objectives; // Objective states
};

struct ObjectiveState {
    uint32_t objectiveIndex;    // Objective number
    bool isCompleted;           // Completion status
    uint32_t currentCount;      // Progress count (if applicable)
    uint32_t targetCount;       // Target count for completion
};
```

## Game Integration

### NVSE Hook System

Quest Sync integrates with Fallout: New Vegas using NVSE hooks to monitor quest events:

#### Quest Stage Changes

```cpp
// Hook into quest stage update function
void __fastcall QuestStageUpdateHook(TESQuest* quest, void* edx, 
                                    UInt32 stage, UInt8 updateGlobal) {
    // Call original function
    ThisStdCall(originalQuestStageUpdate, quest, stage, updateGlobal);
    
    // Notify our tracker
    if (quest && QuestStateTracker::IsTracking()) {
        QuestStateTracker::OnQuestStageChange(quest->refID, stage);
    }
}
```

#### Quest Completion

```cpp
// Hook into quest completion function
void __fastcall QuestCompleteHook(TESQuest* quest, void* edx) {
    // Call original function
    ThisStdCall(originalQuestComplete, quest);
    
    // Notify our tracker
    if (quest && QuestStateTracker::IsTracking()) {
        QuestStateTracker::OnQuestComplete(quest->refID);
    }
}
```

### Script Function Integration

Quest Sync also monitors script-based quest updates:

```cpp
// Monitor SetStage script function calls
bool Cmd_SetStage_Execute(COMMAND_ARGS) {
    UInt32 stage = 0;
    TESQuest* quest = nullptr;
    
    // Extract parameters
    if (ExtractArgs(EXTRACT_ARGS, &stage, &quest)) {
        // Apply stage change
        quest->SetCurrentStageID(stage);
        
        // Notify tracker
        QuestStateTracker::OnQuestStageChange(quest->refID, stage);
        return true;
    }
    return false;
}
```

## Quest Data Management

### Quest Identification

#### Form ID System

Fallout: New Vegas uses Form IDs to uniquely identify game objects:

- **Base Game Quests**: Form IDs 0x00000000 - 0x00FFFFFF
- **DLC Quests**: Form IDs with DLC-specific prefixes
- **Mod Quests**: Form IDs assigned by mod load order

#### Quest Compatibility

```cpp
bool IsQuestCompatible(uint32_t questId) {
    // Check if quest exists in current game setup
    TESQuest* quest = (TESQuest*)LookupFormByID(questId);
    if (!quest) return false;
    
    // Verify quest is from compatible source
    UInt8 modIndex = (questId >> 24) & 0xFF;
    return IsModLoaded(modIndex);
}
```

### Quest State Validation

#### Change Validation

Before applying quest updates, the system validates:

1. **Quest Existence**: Verify quest exists in current game
2. **Stage Progression**: Ensure stage changes are logical
3. **Prerequisite Checks**: Verify quest prerequisites are met
4. **Conflict Resolution**: Handle conflicting updates

```cpp
bool ValidateQuestUpdate(const QuestUpdate& update) {
    TESQuest* quest = GetQuestByID(update.questId);
    if (!quest) return false;
    
    // Check stage progression
    if (update.newStage < quest->currentStage) {
        // Backward progression - validate if allowed
        return quest->AllowsStageRollback();
    }
    
    // Check prerequisites for new stage
    return quest->ArePrerequisitesMet(update.newStage);
}
```

#### Conflict Resolution

When multiple clients update the same quest simultaneously:

1. **Timestamp Priority**: Use server timestamp to determine order
2. **Stage Advancement**: Higher stages take precedence
3. **Completion Priority**: Quest completion overrides stage changes
4. **Rollback Prevention**: Prevent unintended quest rollbacks

## Synchronization Process

### Outbound Synchronization

#### Change Detection

```cpp
void QuestStateTracker::OnQuestStageChange(uint32_t questId, uint32_t newStage) {
    std::lock_guard<std::mutex> lock(questMutex);
    
    auto it = trackedQuests.find(questId);
    if (it != trackedQuests.end()) {
        QuestState& state = it->second;
        
        // Check if this is actually a change
        if (state.currentStage != newStage) {
            uint32_t previousStage = state.currentStage;
            state.currentStage = newStage;
            state.lastUpdate = GetCurrentTimestamp();
            
            // Create and send update message
            QuestUpdate update;
            update.questId = questId;
            update.newStage = newStage;
            update.previousStage = previousStage;
            update.timestamp = state.lastUpdate;
            
            NetworkClient::SendQuestUpdate(update);
        }
    }
}
```

#### Message Creation

```cpp
Message CreateQuestUpdateMessage(const QuestUpdate& update) {
    Message msg;
    msg.type = MessageType::QUEST_STAGE_UPDATE;
    msg.timestamp = GetCurrentTimestamp();
    
    // Serialize quest data
    BinaryWriter writer;
    writer.WriteUInt32(update.questId);
    writer.WriteUInt32(update.newStage);
    writer.WriteUInt32(update.previousStage);
    writer.WriteUInt64(update.timestamp);
    
    // Add objective updates if any
    writer.WriteUInt32(update.objectives.size());
    for (const auto& obj : update.objectives) {
        writer.WriteUInt32(obj.objectiveIndex);
        writer.WriteBool(obj.isCompleted);
        writer.WriteUInt32(obj.currentCount);
        writer.WriteUInt32(obj.targetCount);
    }
    
    msg.payload = writer.GetData();
    return msg;
}
```

### Inbound Synchronization

#### Message Processing

```cpp
void QuestStateTracker::ProcessQuestUpdate(const QuestUpdate& update) {
    // Validate update
    if (!ValidateQuestUpdate(update)) {
        LogWarning("Invalid quest update received: Quest %08X", update.questId);
        return;
    }
    
    // Check if we should apply this update
    if (ShouldApplyUpdate(update)) {
        ApplyQuestUpdate(update);
    }
}
```

#### Game State Application

```cpp
void QuestStateTracker::ApplyQuestUpdate(const QuestUpdate& update) {
    TESQuest* quest = GetQuestByID(update.questId);
    if (!quest) return;
    
    // Temporarily disable our tracking to prevent loops
    ScopedTrackingDisable disableTracking;
    
    // Apply stage change
    if (quest->currentStage != update.newStage) {
        quest->SetCurrentStageID(update.newStage);
        
        // Update objectives
        for (const auto& objUpdate : update.objectives) {
            BGSQuestObjective* objective = quest->GetObjective(objUpdate.objectiveIndex);
            if (objective) {
                objective->SetCompleted(objUpdate.isCompleted);
                if (objUpdate.targetCount > 0) {
                    objective->SetCurrentCount(objUpdate.currentCount);
                }
            }
        }
        
        // Trigger quest update events
        quest->TriggerUpdateEvent();
    }
    
    // Update our tracking state
    UpdateTrackedQuest(update);
}
```

## Quest Types and Compatibility

### Vanilla Quests

#### Main Quest Line

- **Form ID Range**: 0x00000000 - 0x00FFFFFF
- **Compatibility**: Universal across all installations
- **Special Handling**: Critical path quests may need special validation

#### Side Quests

- **Faction Quests**: NCR, Legion, Brotherhood, etc.
- **Companion Quests**: Personal companion storylines
- **Location Quests**: Settlement and location-specific quests

### DLC Quests

#### Dead Money
- **Form ID Prefix**: 0x01000000
- **Compatibility**: Requires Dead Money DLC

#### Honest Hearts
- **Form ID Prefix**: 0x02000000
- **Compatibility**: Requires Honest Hearts DLC

#### Old World Blues
- **Form ID Prefix**: 0x03000000
- **Compatibility**: Requires Old World Blues DLC

#### Lonesome Road
- **Form ID Prefix**: 0x04000000
- **Compatibility**: Requires Lonesome Road DLC

### Mod Quests

#### Compatibility Challenges

1. **Load Order Dependency**: Form IDs depend on mod load order
2. **Version Differences**: Different mod versions may have different quest structures
3. **Missing Mods**: Clients may not have the same mods installed

#### Handling Strategy

```cpp
bool IsModQuestCompatible(uint32_t questId) {
    UInt8 modIndex = (questId >> 24) & 0xFF;
    
    // Check if mod is loaded
    if (!IsModLoaded(modIndex)) {
        return false;
    }
    
    // Get mod information
    ModInfo* modInfo = GetModInfo(modIndex);
    if (!modInfo) return false;
    
    // Check mod version compatibility
    return IsModVersionCompatible(modInfo);
}
```

## Performance Considerations

### Optimization Strategies

#### Selective Tracking

- **Active Quests Only**: Track only currently active quests
- **Player Proximity**: Prioritize quests relevant to current location
- **Quest Importance**: Focus on main storyline and important side quests

#### Batching Updates

```cpp
class QuestUpdateBatcher {
private:
    std::vector<QuestUpdate> pendingUpdates;
    std::chrono::steady_clock::time_point lastBatch;
    static constexpr auto BATCH_INTERVAL = std::chrono::milliseconds(100);

public:
    void AddUpdate(const QuestUpdate& update) {
        pendingUpdates.push_back(update);
        
        auto now = std::chrono::steady_clock::now();
        if (now - lastBatch >= BATCH_INTERVAL) {
            FlushBatch();
        }
    }
    
    void FlushBatch() {
        if (!pendingUpdates.empty()) {
            NetworkClient::SendBatchedUpdates(pendingUpdates);
            pendingUpdates.clear();
            lastBatch = std::chrono::steady_clock::now();
        }
    }
};
```

### Memory Management

#### Quest State Caching

- **LRU Cache**: Keep recently accessed quest states in memory
- **Periodic Cleanup**: Remove inactive quest states
- **Memory Limits**: Configurable memory usage limits

#### Resource Monitoring

```cpp
class QuestTrackerStats {
public:
    size_t trackedQuestCount = 0;
    size_t memoryUsage = 0;
    size_t updatesPerSecond = 0;
    
    void UpdateStats() {
        trackedQuestCount = QuestStateTracker::GetTrackedQuestCount();
        memoryUsage = CalculateMemoryUsage();
        updatesPerSecond = CalculateUpdateRate();
    }
};
```

## Error Handling and Recovery

### Common Issues

#### Quest State Desynchronization

**Causes:**
- Network interruptions during quest updates
- Game crashes during quest progression
- Mod conflicts affecting quest behavior

**Recovery:**
```cpp
void QuestStateTracker::ResynchronizeQuest(uint32_t questId) {
    // Request current state from server
    NetworkClient::RequestQuestState(questId);
    
    // Compare with local state when response received
    // Apply corrections if necessary
}
```

#### Invalid Quest Updates

**Validation Failures:**
- Quest doesn't exist in current game
- Stage progression violates quest logic
- Prerequisites not met for stage change

**Handling:**
```cpp
void HandleInvalidQuestUpdate(const QuestUpdate& update, const std::string& reason) {
    LogWarning("Invalid quest update: %s", reason.c_str());
    
    // Request resynchronization
    NetworkClient::RequestQuestResync(update.questId);
    
    // Notify user if critical quest
    if (IsCriticalQuest(update.questId)) {
        ShowUserNotification("Quest synchronization issue detected");
    }
}
```

## Future Enhancements

### Advanced Features

#### Conditional Synchronization

- **Player Choice Tracking**: Synchronize dialogue choices and decisions
- **Branching Paths**: Handle different quest paths between players
- **Consequence Sharing**: Share quest consequences across players

#### Smart Conflict Resolution

- **Vote-Based Resolution**: Let players vote on conflicting quest states
- **Leader-Based**: Designate quest leader for specific questlines
- **Merge Strategies**: Intelligent merging of conflicting updates

#### Performance Improvements

- **Delta Synchronization**: Send only changed quest data
- **Compression**: Compress quest update messages
- **Predictive Caching**: Pre-cache likely quest progressions

For implementation details, see the source files in `nvse_plugin_example/QuestStateTracker.cpp` and related networking components.
