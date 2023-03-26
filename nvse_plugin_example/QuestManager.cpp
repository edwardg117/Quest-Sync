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

void QuestManager::Add(BGSQuestObjective* Objective, QSyncMsgBody* MessageForServer)
{
	// Quest should already exist in pip-boy, does it exist here?
	if (!this->hasQuest(Objective->quest))
	{
		// Quest is completely new
		cQuest new_quest = cQuest(Objective);
		AllQuests.push_back(new_quest);
		if (new_quest.isActive()) { ActiveQuests.push_back(Objective->quest); }
		if(!new_quest.isObjectiveCompleted(Objective)) { ActiveObjectives.push_back(Objective); }

		ObjectiveCount++;
		_MESSAGE("%s '%s' stage: %s added!", int_to_hex_string(new_quest.getID()).c_str(), new_quest.getName().c_str(), std::to_string(Objective->objectiveId).c_str());
		
		if(MessageForServer != nullptr)
		{
			json quest_info =
			{
				{"ID", int_to_hex_string(new_quest.getID())},
				{"Stage", std::to_string(Objective->objectiveId)},
				{"Name", new_quest.getName()},
				//{"Flags", flagString}
			};
			MessageForServer->type = message_type::NEW_QUEST;
			MessageForServer->body = quest_info;
		}
	}
	else if (!this->hasStage(Objective))
	{
		// Quest is already known, add stage
		cQuest quest = getcQuest(Objective);
		quest.addObjective(Objective);
		if (!quest.isObjectiveCompleted(Objective)) { ActiveObjectives.push_back(Objective); }
		ObjectiveCount++;
		_MESSAGE("%s '%s' new stage: %s", int_to_hex_string(quest.getID()).c_str(), quest.getName().c_str(), std::to_string(Objective->objectiveId).c_str());
		
		if (MessageForServer != nullptr)
		{
			json quest_info =
			{
				{"ID", int_to_hex_string(quest.getID())},
				{"Stage", std::to_string(Objective->objectiveId)},
				{"Name", quest.getName()},
				//{"Flags", flagString}
			};
			MessageForServer->type = message_type::QUEST_UPDATED;
			MessageForServer->body = quest_info;
		}
	}
	else
	{
		// Already have, do not want
		cQuest quest = getcQuest(Objective);
		_MESSAGE("%s '%s' stage: %s is not new and already exists in the quest manager.", int_to_hex_string(quest.getID()).c_str(), quest.getName().c_str(), std::to_string(Objective->objectiveId).c_str());
	}
}

void QuestManager::Add(std::string refID, NVSEConsoleInterface* g_consoleInterface)
{
	// Quest in pip-boy?
	if (!this->hasQuest(refID))
	{
		// Quest is completely new
		// Can only add quest here atm
		std::string startquest = "StartQuest " + refID;
		g_consoleInterface->RunScriptLine(startquest.c_str(), nullptr);
	}
	else
	{
		// Already have, do not want
	}
}

void QuestManager::Add(std::string refID, std::string ObjectiveId, NVSEConsoleInterface* g_consoleInterface)
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

void QuestManager::removeActiveQuest(std::string refID)
{
	auto active_quest_iterator = ActiveQuests.begin();
	while (active_quest_iterator != ActiveQuests.end())
	{
		//TESQuest* quest = (*active_quest_iterator); // Doing this for readability sake
		cQuest Quest(*active_quest_iterator); // Doing this for readability sake
		if (int_to_hex_string(Quest.getID()) == refID) // Has the active flag been removed or the completed flag been set?
		{
			_MESSAGE("Removed %s %s from the active quest list", refID.c_str(), Quest.getName().c_str());
			ActiveQuests.erase(active_quest_iterator++); // First increments the iterator with ++, then removes the previous node as using ++ returns the node you are incrementing from

			// If quest is being removed, also remove the objectives
			for (auto objective : Quest.getObjectives())
			{
				removeActiveObjective(refID, std::to_string(objective->objectiveId)); // TODO this will pass objectives that aren't being monitored, don't care right now...
			}

			break;
		}
		else
		{
			++active_quest_iterator; // Move to the next node, this one is still active
		}
	}
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

void QuestManager::removeActiveObjective(std::string refID, std::string objectiveId)
{
	auto active_objective_iterator = ActiveObjectives.begin();
	while (active_objective_iterator != ActiveObjectives.end())
	{
		BGSQuestObjective* objective = (*active_objective_iterator);
		cQuest Quest(objective);
		if (int_to_hex_string(Quest.getID()) == refID && std::to_string(objective->objectiveId) == objectiveId) // If the completed flag is set
		{
			
			_MESSAGE("Removed stage %s, %s %s from the active objective list", std::to_string(objective->objectiveId), refID.c_str(), Quest.getName().c_str());
			// Remove the objective from the list
			ActiveObjectives.erase(active_objective_iterator++); // First increments the iterator with ++, then removes the previous node, as using ++ returns the node you are incrementing from
			break;
		}
		else
		{
			++active_objective_iterator;
		}
	}
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
	ActiveObjectives.clear();
	ObjectiveCount = 0;
	ConnAckReceived = false;
	SyncedWithServer = false;
	SyncState = 0;
}

bool QuestManager::completedIntro()
{
	if(this->hasQuest("104c1c"))
	{
		cQuest AintThatAKickInTheHead = this->getcQuest("104c1c");
		return AintThatAKickInTheHead.isComplete();
	}
	else { return false; }
}

bool QuestManager::isConnectionAcknowledged()
{
	return this->ConnAckReceived;
}

bool QuestManager::isSyncedWithServer()
{
	return this->SyncedWithServer;
}

UInt8 QuestManager::getSyncState()
{
	return this->SyncState;
}

void QuestManager::syncWithServer(TCPClient* client)
{
	QSyncMsgBody msgBody(message_type::REQUEST_CURRENT_QUESTS_COMPLETION, {});
	QSyncMessage message;
	message.addMessage(msgBody);
	client->send_message(message.toString());
	this->SyncState = 1;
}

void QuestManager::populate_current_quests(tList<BGSQuestObjective> questObjectiveList)
{
	auto iterator = questObjectiveList.Head(); // This is what I'll use to iterate through the list (sort of)
	//auto iterator = std::make_reverse_iterator(questObjectiveList);
	_MESSAGE("Re-populating the current quest and objective lists...");
	AllQuests.clear(); // Just in case this is called while there's already stuff in it, this function shouldn't cause problems by resetting the positions of these things in any case
	ActiveQuests.clear();
	ActiveObjectives.clear();
	ObjectiveCount = 0;
	std::list<BGSQuestObjective*> reversed_list;

	for (int current_new_quest = 0; current_new_quest < questObjectiveList.Count(); current_new_quest++) // Do ALL quest entries in the pip-boy
	{
		reversed_list.push_front(iterator->data);
		iterator = iterator->next;
	}

	for (auto objective : reversed_list)
	{
		this->Add(objective, nullptr);
	}
}

void QuestManager::process(TCPClient* client, tList<BGSQuestObjective> questObjectiveList, NVSEConsoleInterface* g_consoleInterface)
{
	// If there are quests in the pip-boy that haven't been added, they should be done first
	QSyncMessage message_for_server;
	if (questObjectiveList.Count() > ObjectiveCount && this->SyncedWithServer)
	{
		// There is a new quest started or a quest has been updated
		// 2.1.1 Get latest addition/s
		UInt32 num_new_quests = questObjectiveList.Count() - ObjectiveCount; // Unint because that's what .Count() returns, also never a negative number so why bother mnaking it signed?
		auto iterator = questObjectiveList.Head(); // This is what I'll use to iterate through the list (sort of)
		_MESSAGE("There's %i new entries in the pip-boy", num_new_quests);
		//QSyncMessage message_for_server;
		for (int current_new_quest = 0; current_new_quest < num_new_quests; current_new_quest++) // Do only new entries in the pip-boy
		{
			QSyncMsgBody message_body;
			this->Add(iterator->data, &message_body);

			if(message_body.type != message_type::NONE)
			{
				message_for_server.addMessage(message_body);
			}

			iterator = iterator->next;
		}

		// Don't update objective count because that's already been done
		//client.send_message(message_for_server.toString());
	}

	auto active_objective_iterator = ActiveObjectives.begin();
	while (active_objective_iterator != ActiveObjectives.end())
	{
		BGSQuestObjective* objective = (*active_objective_iterator);
		cQuest Quest(objective);
		if (Quest.isObjectiveCompleted(objective)) // If the completed flag is set
		{
			UInt32 StageID = objective->objectiveId;

			_MESSAGE("%s stage %i completed.", Quest.getName().c_str(), StageID);
			QSyncMsgBody msg;
			msg.type = message_type::OBJECTIVE_COMPLETED;
			json stage_info =
			{
				{"Stage", std::to_string(StageID)},
				{"ID", int_to_hex_string(Quest.getID())}
			};
			msg.body = stage_info;
			message_for_server.addMessage(msg);

			// Remove the objective from the list
			ActiveObjectives.erase(active_objective_iterator++); // First increments the iterator with ++, then removes the previous node, as using ++ returns the node you are incrementing from
		}
		else
		{
			++active_objective_iterator;
		}
	}

	auto active_quest_iterator = ActiveQuests.begin();
	while (active_quest_iterator != ActiveQuests.end())
	{
		//TESQuest* quest = (*active_quest_iterator); // Doing this for readability sake
		cQuest Quest(*active_quest_iterator); // Doing this for readability sake
		if (!Quest.isActive() || Quest.isComplete()) // Has the active flag been removed or the completed flag been set?
		{
			// Current quest is no longer active
			// Was the quest completed or failed?
			if (Quest.isComplete() || Quest.isFailed()) // Has the quest been (completed or failed) or was the active flag just removed for some reason?
			{
				if (!Quest.isFailed())
				{
					_MESSAGE("%s Completed!", Quest.getName().c_str());
					QSyncMsgBody msg;
					msg.type = message_type::QUEST_COMPLETED;
					json quest_info =
					{
						{"ID", int_to_hex_string(Quest.getID())},
						{"Name", Quest.getName()},
						//{"Flags", flagString}
					};
					msg.body = quest_info;
					message_for_server.addMessage(msg);
				}
				else
				{
					_MESSAGE("%s Failed!", Quest.getName().c_str());
					QSyncMsgBody msg;
					msg.type = message_type::QUEST_FAILED;
					json quest_info =
					{
						{"ID", int_to_hex_string(Quest.getID())},
						{"Name", Quest.getName()},
						//{"Flags", flagString}
					};
					msg.body = quest_info;
					message_for_server.addMessage(msg);
				}
			}
			else
			{
				_MESSAGE("Quest '%s' '%s' Has been removed from the active list and is not completed or failed! %s", int_to_hex_string(Quest.getID()).c_str(), Quest.getName().c_str(), Quest.getFlagString().c_str());
				QSyncMsgBody msg;
				msg.type = message_type::QUEST_INACTIVE;
				json quest_info =
				{
					{"ID", int_to_hex_string(Quest.getID())},
					{"Name", Quest.getName()},
					{"Flags", Quest.getFlagString()}
				};
				msg.body = quest_info;
				message_for_server.addMessage(msg);
			}
			// Remove this quest from the list
			ActiveQuests.erase(active_quest_iterator++); // First increments the iterator with ++, then removes the previous node as using ++ returns the node you are incrementing from
		}
		else
		{
			++active_quest_iterator; // Move to the next node, this one is still active
		}
	}


	// Process actions from server
	std::list<std::string> messages = client->GetMessages();
	for (auto message : messages)
	{
		_MESSAGE("Received %i message(s), parsing!", messages.size());
		QSyncMessage msg(message);
		for (auto instruction : msg.getMessages())
		{
			message_type Type;
			instruction["Type"].get_to(Type);
			if (!this->completedIntro()) // Don't sync quests until this is done
			{
				if (Type != message_type::CONNECTION_ACKNOWLEDGEMENT) { _MESSAGE("Ignored message from server as a save hasn't been loaded yet. Or player hasn't completed 'Aint that a kick in the head'");  break; } // Don't do anything other than a connection acknowledgement if these are true
			}
			switch (Type)
			{
			case message_type::CONNECTION_ACKNOWLEDGEMENT:
			{
				_MESSAGE("Server sent Connection Acknowledgement");
				std::string msg_from_server;
				instruction["Body"]["Message"].get_to(msg_from_server);
				_MESSAGE("Message From Server: %s", msg_from_server.c_str());
				ConnAckReceived = true;
			}
			break;
			case message_type::START_QUEST:
			{
				_MESSAGE("Server wants me to start a new quest:");
				auto quest_info = instruction["Body"];
				std::string ID;
				quest_info["ID"].get_to(ID);
				std::string Name;
				quest_info["Name"].get_to(Name);
				std::string Stage;
				quest_info["Stage"].get_to(Stage);
				//std::string Flags;
				//quest_info["Flags"].get_to(Flags);
				_MESSAGE("%s %s %s", ID.c_str(), Name.c_str(), Stage.c_str());
				// TODO investigate why this doesn't display the quest for specific quests
				this->Add(ID, Stage, g_consoleInterface); // Add through console
				this->Add(questObjectiveList.Head()->data, nullptr); // Add the newly aquired quest here (does nothing if quest already exists)
			}
			break;
			case message_type::UPDATE_QUEST:
			{
				_MESSAGE("Server wants me to progress a quest:");
				auto quest_info = instruction["Body"];
				std::string ID;
				quest_info["ID"].get_to(ID);
				std::string Name;
				quest_info["Name"].get_to(Name);
				std::string Stage;
				quest_info["Stage"].get_to(Stage);
				//std::string Flags;
				//quest_info["Flags"].get_to(Flags);
				_MESSAGE("%s %s %s", ID.c_str(), Name.c_str(), Stage.c_str());
				this->Add(ID, Stage, g_consoleInterface); // Add through console
				this->Add(questObjectiveList.Head()->data, nullptr); // Add the newly aquired quest here (does nothing if quest already exists)
			}
			break;
			case message_type::COMPLETE_OBJECTIVE:
			{
				_MESSAGE("Server has told me to complete a quest objective");
				auto stage_info = instruction["Body"];
				//SetObjectiveCompleted Quest:baseform objectiveIndex:int completedFlag:int{0/1}
				std::string ID;
				stage_info["ID"].get_to(ID);
				std::string Stage;
				stage_info["Stage"].get_to(Stage);
				_MESSAGE("%s %s", ID.c_str(), Stage.c_str());
				std::string completeStage = "SetObjectiveCompleted " + ID + " " + Stage + " 1";
				g_consoleInterface->RunScriptLine(completeStage.c_str(), nullptr);
				this->removeActiveObjective(ID, Stage);
				// TODO figure out how much XP should be earned

			}

			break;
			case message_type::COMPLETE_QUEST:
			{
				_MESSAGE("Server wants me to complete quest:");
				auto quest_info = instruction["Body"];
				std::string ID;
				quest_info["ID"].get_to(ID);
				std::string Name;
				quest_info["Name"].get_to(Name);
				std::string Stage;
				quest_info["Stage"].get_to(Stage);
				std::string Flags;
				quest_info["Flags"].get_to(Flags);
				_MESSAGE("%s %s %s %s", ID.c_str(), Name.c_str(), Stage.c_str(), Flags.c_str());
				std::string complete = "CompleteQuest " + ID;
				g_consoleInterface->RunScriptLine(complete.c_str(), nullptr);
				this->removeActiveQuest(ID);
				// TODO figure out how much XP should be earned
			}
			break;
			case message_type::FAIL_QUEST:
			{
				_MESSAGE("Server wants me to fail quest:");
				auto quest_info = instruction["Body"];
				std::string ID;
				quest_info["ID"].get_to(ID);
				std::string Name;
				quest_info["Name"].get_to(Name);
				std::string Stage;
				quest_info["Stage"].get_to(Stage);
				std::string Flags;
				quest_info["Flags"].get_to(Flags);
				_MESSAGE("%s %s %s %s", ID.c_str(), Name.c_str(), Stage.c_str(), Flags.c_str());
				std::string fail = "FailQuest " + ID;
				g_consoleInterface->RunScriptLine(fail.c_str(), nullptr);
				this->removeActiveQuest(ID);
			}
			break;
			case message_type::CURRENT_ACTIVE_QUESTS:
			{
				// Server has sent me a list of all quests and objectives that I should have active
				_MESSAGE("Sever has replied with current active quests and objectives");
				json quest_list = instruction["Body"];
				std::string ID;
				std::string Name;
				std::list<std::string> stages;
				std::vector<std::string> stage_list;
				_MESSAGE("Expecting a crash here...");
				//quest_list.get_to(stage_list);
				_MESSAGE("%s", quest_list.dump().c_str());
				_MESSAGE("Going to loop through the list of quests and their objectives");
				for (auto quest : quest_list)
				{
					//_MESSAGE(quest_s.c_str());
					_MESSAGE("Or here???");
					//auto quest = json::parse(quest_s);
					_MESSAGE("Quest parsed...");
					quest["ID"].get_to(ID);
					quest["Name"].get_to(Name);
					quest["Stages"].get_to(stages);

					_MESSAGE("Extracted info, checking against the current lists...");
					// Let Add decide what to do with this information
					for (auto objective : stages)
					{
						this->Add(ID, objective, g_consoleInterface); // Add through console
						this->Add(questObjectiveList.Head()->data, nullptr); // Add the newly aquired quest here (does nothing if quest already exists)
					}
				}
				// Create a list of current quests and objectives
				json current_quests = json::array();
				for (auto objective : ActiveObjectives)
				{
					std::string quest_name = objective->quest->GetFullName() ? objective->quest->GetFullName()->name.CStr() : "<no name>";
					json quest_info =
					{
						{"ID", int_to_hex_string(objective->quest->refID)},
						{"Stage", std::to_string(objective->objectiveId)},
						{"Name", quest_name},
						//{"Flags", flagString}
					};
					_MESSAGE("Telling the server about %s stage %s", quest_name.c_str(), std::to_string(objective->objectiveId).c_str());
					current_quests.push_back(quest_info);
				}
				QSyncMsgBody message_body(message_type::ACTIVE_QUESTS, current_quests);
				message_for_server.addMessage(message_body);


				this->SyncedWithServer = true;
				this->SyncState = 2;

			}
			break;
			case message_type::ALL_QUEST_STATES: _MESSAGE("ALL_QUEST_STATES not implemented!"); break;
			case message_type::RESEND_CONN_ACK: _MESSAGE("RESEND_CONN_ACK not implemented!"); break;
			default:
				_MESSAGE("Unexpected message_type!");

			}
		}

	}


	if (message_for_server.getSize() > 0) // If there's any messaged to send to the server, send em!
	{
		std::string a_test = message_for_server.toString();
		_MESSAGE("Sending:\n%s", a_test);
		client->send_message(a_test);
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
	return has_objective;
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
	return has_objective;
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
	return Quest->GetFullName() ? Quest->GetFullName()->name.CStr() : "<no name>";
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
