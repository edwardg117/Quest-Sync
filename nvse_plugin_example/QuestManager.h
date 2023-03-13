#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "nvse/GameForms.h"
#include "nvse/PluginAPI.h"
#include "TCPClient.h"
#include "Quest Sync Server/QsyncDefinitions.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

NVSEConsoleInterface* g_consoleInterface{};

class cQuest
{
public:
	cQuest(TESQuest* Quest);
	cQuest(BGSQuestObjective* Objective);
	~cQuest();

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
	std::list<cQuest> ActiveQuests;
	//std::list<BGSQuestObjective*> ActiveObjectives;
	UInt32 ObjectiveCount;

	std::string int_to_hex_string(int number);
	//void Add(cQuest Quest);
	void Add(BGSQuestObjective* Objective, std::vector<std::string>* MessageForServer);
	void Add(std::string refID, std::string ObjectiveId);

public:
	QuestManager();

	~QuestManager();

	bool hasQuest(std::string refID);
	bool hasQuest(UInt32 refID);
	bool hasQuest(TESQuest* Quest);
	bool hasStage(std::string QuestID, std::string objectiveId);
	bool hasStage(UInt32 refID, UInt32 objectiveId);
	bool hasStage(BGSQuestObjective* Objective);
	cQuest getcQuest(std::string refID);
	cQuest getcQuest(BGSQuestObjective* Objective);
	void reset();
	void process(TCPClient client, tList<BGSQuestObjective>	questObjectiveList);

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