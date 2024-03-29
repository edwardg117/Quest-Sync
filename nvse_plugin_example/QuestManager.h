#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "nvse/GameForms.h"
#include "nvse/PluginAPI.h"
#include "nvse/utility.h"
#include "nvse/PluginAPI.h"
#include "nvse/CommandTable.h"
#include "nvse/GameAPI.h"
#include "nvse/ParamInfos.h"
#include "nvse/GameObjects.h"

#include "TCPClient.h"
#include "Quest Sync Server/QsyncDefinitions.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class cQuest
{
public:
	cQuest(TESQuest* Quest);
	cQuest(BGSQuestObjective* Objective);
	//~cQuest();

	UInt32 getID();
	std::vector<BGSQuestObjective*> getObjectives();
	bool hasObjective(std::string objectiveId);
	bool hasObjective(BGSQuestObjective* Objective);
	std::vector<bool> getFlagStates();
	std::string getFlagString();
	void addObjective(BGSQuestObjective* Objective);
	std::string getName();
	bool isActive();
	bool isComplete();
	bool isFailed();
	bool isObjectiveCompleted(BGSQuestObjective* Objective);
private:
	TESQuest* Quest;
	std::vector<BGSQuestObjective*> Objectives;
	void setQuest(TESQuest* Quest);
};

class QuestManager
{
private:
	std::vector<cQuest> AllQuests;
	std::list<TESQuest*> ActiveQuests;
	std::list<BGSQuestObjective*> ActiveObjectives;
	UInt32 ObjectiveCount;
	bool ConnAckReceived;
	bool SyncedWithServer;
	UInt8 SyncState;

	std::string int_to_hex_string(int number);
	//void Add(cQuest Quest);
	void Add(BGSQuestObjective* Objective, QSyncMsgBody* MessageForServer);
	void Add(std::string refID, NVSEConsoleInterface* g_consoleInterface);
	void Add(std::string refID, std::string ObjectiveId, NVSEConsoleInterface* g_consoleInterface);
	void removeActiveQuest(std::string refID);
	void removeActiveObjective(std::string refID, std::string objectiveId);

public:
	QuestManager();

	~QuestManager();

	bool hasQuest(std::string refID);
	bool hasQuest(UInt32 refID);
	bool hasQuest(TESQuest* Quest);
	bool hasStage(std::string QuestID, std::string objectiveId);
	bool hasStage(UInt32 refID, UInt32 objectiveId);
	bool hasStage(BGSQuestObjective* Objective);
	bool completedIntro();
	cQuest getcQuest(std::string refID);
	cQuest getcQuest(BGSQuestObjective* Objective);
	void reset();
	void process(TCPClient* client, tList<BGSQuestObjective> questObjectiveList, NVSEConsoleInterface* g_consoleInterface);
	bool isSyncedWithServer();
	bool isConnectionAcknowledged();
	UInt8 getSyncState();
	void syncWithServer(TCPClient* client);
	void populate_current_quests(tList<BGSQuestObjective> questObjectiveList);
};

class QuestNotInManagerException : public std::exception {
private:
	std::string message;
	std::string int_to_hex_string(int number)
	{
		std::stringstream stream;
		stream << std::hex << number;
		return std::string(stream.str());
	}
public:
	QuestNotInManagerException(UInt32 refID) : message("The Quest " + this->int_to_hex_string(refID) + " does not exist in the QuestManager!") {}
	QuestNotInManagerException(std::string refID) : message("The Quest " + refID + " does not exist in the QuestManager!") {}

	const char* what() {
		return message.c_str();
	}
};