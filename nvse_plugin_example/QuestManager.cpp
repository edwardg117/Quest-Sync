#include "QuestManager.h"

std::string QuestManager::int_to_hex_string(int number)
{
	std::stringstream stream;
	stream << std::hex << number;
	return std::string(stream.str());
}
/*void QuestManager::Add(cQuest Quest)
{
	// Quest should already exist in pip-boy, does it exist here?
	if (!this->hasQuest(Quest.getID()))
	{
		// Quest is completely new
		AllQuests.push_back(Quest);
		ActiveQuests.push_back(Quest);

		ObjectiveCount++;
		// TODO Tell server
	}
	else if (!this->hasStage(Objective))
	{
		// Quest is already known, add stage
		this->getcQuest(Objective).addObjective(Objective);
		ObjectiveCount++;
		// TODO Tell server
	}
	else
	{
		// Already have, do not want
	}
}*/

void QuestManager::Add(BGSQuestObjective* Objective, std::vector<std::string>* MessageForServer)
{
	// Quest should already exist in pip-boy, does it exist here?
	if (!this->hasQuest(Objective->quest))
	{
		// Quest is completely new
		cQuest new_quest = cQuest(Objective);
		AllQuests.push_back(new_quest);
		ActiveQuests.push_back(new_quest);

		ObjectiveCount++;
		_MESSAGE("%s '%s' stage: %s added!", int_to_hex_string(new_quest.getID()).c_str(), new_quest.getName().c_str(), std::to_string(Objective->objectiveId).c_str());
		// TODO Tell server
		json quest_info =
		{
			{"ID", int_to_hex_string(new_quest.getID())},
			{"Stage", std::to_string(Objective->objectiveId)},
			{"Name", new_quest.getName()},
			//{"Flags", flagString}
		};
		MessageForServer->push_back(quest_info.dump());
	}
	else if (!this->hasStage(Objective))
	{
		// Quest is already known, add stage
		cQuest quest = getcQuest(Objective);
		quest.addObjective(Objective);
		ObjectiveCount++;
		_MESSAGE("%s '%s' new stage: %s", int_to_hex_string(quest.getID()).c_str(), quest.getName().c_str(), std::to_string(Objective->objectiveId).c_str());
		// TODO Tell server
		json quest_info =
		{
			{"ID", int_to_hex_string(quest.getID())},
			{"Stage", std::to_string(Objective->objectiveId)},
			{"Name", quest.getName()},
			//{"Flags", flagString}
		};
		MessageForServer->push_back(quest_info.dump());
	}
	else
	{
		// Already have, do not want
		cQuest quest = getcQuest(Objective);
		_MESSAGE("%s '%s' stage: %s is not new and already exists in the quest manager.", int_to_hex_string(quest.getID()).c_str(), quest.getName().c_str(), std::to_string(Objective->objectiveId).c_str());
	}
}

void QuestManager::Add(std::string refID, std::string ObjectiveId)
{
	// Quest in pip-boy?
	if (!this->hasQuest(refID))
	{
		// Quest is completely new
		// Can only add quest here atm
		std::string startquest = "StartQuest " + refID;
		std::string setstage = "SetStage " + refID + " " + ObjectiveId;
		g_consoleInterface->RunScriptLine(setstage.c_str(), nullptr);
		// TODO Tell server
	}
	else if (!this->hasStage(refID, ObjectiveId))
	{
		// Quest is already known, add stage
		//this->getcQuest(refID).addObjective(Objective);
		// Can only add new objective here
		std::string setstage = "SetStage " + refID + " " + ObjectiveId;
		// Updating a quest to the next stage usually makes it visible if it isn't already for some reason
		g_consoleInterface->RunScriptLine(setstage.c_str(), nullptr);
		// TODO Tell server
	}
	else
	{
		// Already have, do not want
	}

}

QuestManager::QuestManager()
{
	reset();
}

QuestManager::~QuestManager()
{
	
}

bool QuestManager::hasQuest(std::string refID)
{
	bool has_quest = false;
	for (auto existing_quest : AllQuests)
	{
		if (int_to_hex_string(existing_quest.getID()) == refID) { has_quest = true; break; }
	}
	return has_quest;
}

bool QuestManager::hasQuest(UInt32 refID)
{
	bool has_quest = false;
	for (auto existing_quest : AllQuests)
	{
		if (existing_quest.getID() == refID) { has_quest = true; break; }
	}
	return has_quest;
}

bool QuestManager::hasQuest(TESQuest* Quest)
{
	bool has_quest = false;
	for (auto existing_quest : AllQuests)
	{
		if (existing_quest.getID() == Quest->refID) { has_quest = true; break; }
	}
	return has_quest;
}

bool QuestManager::hasStage(std::string QuestID, std::string objectiveId)
{
	bool has_objective = false;
	for (auto quest : AllQuests)
	{
		if (int_to_hex_string(quest.getID()) == QuestID) // Find quest in list
		{ 
			if(quest.hasObjective(objectiveId))
			{
				has_objective = true;
			}
			break;
		}
	}
	return has_objective;
}

bool QuestManager::hasStage(UInt32 refID, UInt32 objectiveId)
{
	bool has_objective = false;
	for (auto quest : AllQuests)
	{
		if (quest.getID() == refID) // Find quest in list
		{
			if (quest.hasObjective(std::to_string(objectiveId)))
			{
				has_objective = true;
			}
			break;
		}
	}
	return has_objective;
}

bool QuestManager::hasStage(BGSQuestObjective* Objective)
{
	bool has_objective = false;
	for (auto quest : AllQuests)
	{
		if (quest.getID() == Objective->quest->refID) // Find quest in list
		{
			if (quest.hasObjective(Objective))
			{
				has_objective = true;
			}
			break;
		}
	}
	return has_objective;
}

cQuest QuestManager::getcQuest(std::string refID)
{
	for (auto quest : AllQuests)
	{
		if (int_to_hex_string(quest.getID()) == refID) // Find quest in list
		{
			return quest;
		}
	}
	throw QuestNotInManagerException(refID); // If going to fuck up, explain why
}

cQuest QuestManager::getcQuest(BGSQuestObjective* Objective)
{
	for (auto quest : AllQuests)
	{
		if (quest.getID() == Objective->quest->refID) // Find quest in list
		{
			return quest;
		}
	}
	throw QuestNotInManagerException(Objective->quest->refID); // If going to fuck up, explain why
}

void QuestManager::reset()
{
	AllQuests.clear();
	ActiveQuests.clear();
	ObjectiveCount = 0;
}

void QuestManager::process(TCPClient client, tList<BGSQuestObjective> questObjectiveList)
{
	// If there are quests in the pip-boy that haven't been added, they should be done first
	if (questObjectiveList.Count() > ObjectiveCount)
	{
		// There is a new quest started or a quest has been updated
		// 2.1.1 Get latest addition/s
		UInt32 num_new_quests = questObjectiveList.Count() - ObjectiveCount; // Unint because that's what .Count() returns, also never a negative number so why bother mnaking it signed?
		auto iterator = questObjectiveList.Head(); // This is what I'll use to iterate through the list (sort of)
		_MESSAGE("There's %i new entries in the pip-boy", num_new_quests);

		for (int current_new_quest = 0; current_new_quest < num_new_quests; current_new_quest++) // Do only new entries in the pip-boy
		{
			//2.1.2 Is this a new quest or an existing one that's now in progress? 
			TESQuest* new_quest = iterator->data->quest; // The quest attached to the current entry
			UInt32 stage = iterator->data->objectiveId; // The quest stage as it appears on the fallout wiki
			cQuest Quest(iterator->data);
			// Create a pretty string to represent the flags "10001100"
			std::string flagString = Quest.getFlagString();
			// Get the quest name as displayed in the pip-boy
			std::string quest_name = Quest.getName();
			std::vector<std::string> message_body = {};
			this->Add(iterator->data, &message_body);
			QSyncMsgBody()

			if (is_new_quest(new_quest->refID) && is_quest_active(new_quest) && !is_quest_complete(new_quest)) // Quest can remain active when complete
			{
				// Quest was just added to the pip-boy
				g_current_quest_list.push_back(new_quest); // Add to the list fo quests I'm monitoring for completeion

				_MESSAGE("New Quest '%s' '%s' stage: %s added! %s", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), std::to_string(stage).c_str(), flagString.c_str());
				// Tell the server about this new quest
				json quest_info =
				{
					{"ID", int_to_hex_string(new_quest->refID)},
					{"Stage", std::to_string(stage)},
					{"Name", quest_name},
					{"Flags", flagString}
				};
				QSyncMessage msg(message_type::NEW_QUEST, quest_info.dump());
				client.send_message(msg.toString());
			}
			else if (is_quest_active(new_quest))
			{
				// Quest already exists in the pip-boy and has only updated
				_MESSAGE("Quest '%s' '%s' updated to stage: %s ! %s", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), std::to_string(stage).c_str(), flagString.c_str());
				// Tell server that this quest updated
				json quest_info =
				{
					{"ID", int_to_hex_string(new_quest->refID)},
					{"Stage", std::to_string(stage)},
					{"Name", quest_name},
					{"Flags", flagString}
				};

				QSyncMessage msg(message_type::QUEST_UPDATED, quest_info.dump());
				client.send_message(msg.toString());
			}
			else
			{
				_MESSAGE("Quest '%s' '%s' stage: %s has appeared but is not active! %s This may not be an issue.", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), std::to_string(stage).c_str(), flagString.c_str());
				// Most likely this quest will be picked activated when it is next updated and I can add it then
			}

			BGSQuestObjective* objective = iterator->data;
			if (is_new_objective(objective) && !is_objective_completed(objective) && !is_quest_complete(new_quest) && !is_quest_failed(new_quest)) // If a quest is failed, the objective is not marked as completed. Check for that too. It might be possible for a quest to be inactive but receive a new objective before it activates
			{
				g_current_objective_list.push_back(objective);
				_MESSAGE("Stage is not marked as complete, adding to the current objective list.");
			}

			iterator = iterator->next;
		}
		// TEMP display current quests being watched
		if (false)
		{
			auto active_quest_iterator = g_current_quest_list.begin();
			while (active_quest_iterator != g_current_quest_list.end())
			{
				TESQuest* quest = (*active_quest_iterator); // Doing this for readability sake
				if (!is_quest_active(quest))
				{
					std::string quest_name = quest->GetFullName() ? quest->GetFullName()->name.CStr() : "<no name>";
					_MESSAGE("Quest '%s' '%s' Inactive!", int_to_hex_string(quest->refID).c_str(), quest_name.c_str());
				}
				else
				{
					std::string quest_name = quest->GetFullName() ? quest->GetFullName()->name.CStr() : "<no name>";
					_MESSAGE("Quest '%s' '%s' Currently Active!", int_to_hex_string(quest->refID).c_str(), quest_name.c_str());
				}
				++active_quest_iterator;
			}
		}

		g_previousQuestCount += num_new_quests; // Update the number of quest objectives that were in the list on this run so I can compare to it next run
	}
}

cQuest::cQuest(TESQuest* Quest)
{
	setQuest(Quest);
}

cQuest::cQuest(BGSQuestObjective* Objective)
{
	setQuest(Objective->quest);
	addObjective(Objective);
}

UInt32 cQuest::getID()
{
	return Quest->refID;
}

std::vector<BGSQuestObjective*> cQuest::getObjectives()
{
	return Objectives;
}

bool cQuest::hasObjective(std::string objectiveId)
{
	bool has_objective = false;
	for (auto objective : Objectives) // Does it have the objective?
	{
		if (objective->objectiveId == stoi(objectiveId))
		{
			has_objective = true;
			break;
		}
	}
}

bool cQuest::hasObjective(BGSQuestObjective* Objective)
{
	bool has_objective = false;
	for (auto objective : Objectives) // Does it have the objective?
	{
		if (objective->objectiveId == Objective->objectiveId)
		{
			has_objective = true;
			break;
		}
	}
}

std::vector<bool> cQuest::getFlagStates()
{
	std::vector<bool> flag_states; // A vector of states for the quest flags

	// Perform a comparison using the AND operator to compare the bits
	// 0000 0000 & 0000 0001 = false
	// 0000 0001 & 0000 0001 = true
	// 0010 0011 & 0000 0001 = true

	flag_states.push_back((Quest->flags & 1) == 1);		// Bit 1 - Active
	flag_states.push_back((Quest->flags & 2) == 2);		// Bit 2 - Completed
	flag_states.push_back((Quest->flags & 4) == 4);		// Bit 3
	flag_states.push_back((Quest->flags & 8) == 8);		// Bit 4
	flag_states.push_back((Quest->flags & 16) == 16);	// Bit 5
	flag_states.push_back((Quest->flags & 32) == 32);	// Bit 6 - Visible in pip-boy
	flag_states.push_back((Quest->flags & 64) == 64);	// Bit 7 - Failed
	flag_states.push_back((Quest->flags & 128) == 128);	// Bit 8

	return flag_states;
}

std::string cQuest::getFlagString()
{
	std::string flagString = "";
	std::vector<bool> quest_flags = this->getFlagStates();
	for (auto flag : quest_flags)
	{
		flagString += flag ? "1" : "0";
	}

	return flagString;
}

void cQuest::addObjective(BGSQuestObjective* Objective)
{
	Objectives.push_back(Objective);
}

std::string cQuest::getName()
{
	std::string quest_name = Quest->GetFullName() ? Quest->GetFullName()->name.CStr() : "<no name>";
}

bool cQuest::isActive()
{
	return (Quest->flags & 1) == 1;
}

bool cQuest::isComplete()
{
	return (Quest->flags & 2) == 2;
}

bool cQuest::isFailed()
{
	return (Quest->flags & 64) == 64;
}

bool cQuest::isObjectiveCompleted(BGSQuestObjective* Objective)
{
	return (Objective->status & 2) == 2;
}

void cQuest::setQuest(TESQuest* Quest)
{
	this->Quest = Quest;
}
