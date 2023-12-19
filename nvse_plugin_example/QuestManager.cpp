#include "QuestManager.h"

void printQuestInfo(BGSQuestObjective* Quest, NVSEConsoleInterface* g_consoleInterface);

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

		ObjectiveCount++;
		_MESSAGE("%s '%s' stage: %s added!", int_to_hex_string(new_quest.getID()).c_str(), new_quest.getName().c_str(), std::to_string(new_quest.getStage()).c_str());

		if (MessageForServer != nullptr)
		{
			QSyncQuest quest_info = QSyncQuest(new_quest.getIDString(), std::to_string(new_quest.getStage()), new_quest.GetStagesAsSimple(), new_quest.isActive(), new_quest.isComplete(), new_quest.isFailed());
			json jsonQuest = {
				{"Quest", quest_info.ToString()}
			};
			MessageForServer->type = message_type::NEW_QUEST;
			MessageForServer->body = jsonQuest;
		}
	}
	else
	{
		// Quest is already known, add stage
		cQuest quest = getcQuest(Objective);
		if (quest.HaveStagesChanged()) {
			ObjectiveCount++;
			_MESSAGE("%s '%s' Stage: %s new objective: %s", int_to_hex_string(quest.getID()).c_str(), quest.getName().c_str(), std::to_string(quest.getStage()).c_str(), std::to_string(Objective->objectiveId).c_str());

			if (MessageForServer != nullptr)
			{
				//quest.
				QSyncQuest quest_info = QSyncQuest(quest.getIDString(), std::to_string(quest.getStage()), quest.GetStagesAsSimple(), quest.isActive(), quest.isComplete(), quest.isFailed());
				json jsonQuest = {
					{"Quest", quest_info.ToString()}
				};
				MessageForServer->type = message_type::QUEST_UPDATED;
				MessageForServer->body = jsonQuest;
			}
		}
		else
		{
			// Already have, do not want
			_MESSAGE("%s '%s' stage: %s is not new and already exists in the quest manager.", int_to_hex_string(quest.getID()).c_str(), quest.getName().c_str(), std::to_string(quest.getStage()).c_str());
		}
		
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

void QuestManager::Add(std::string refID, std::string StageId, NVSEConsoleInterface* g_consoleInterface)
{
	// Quest in pip-boy?
	if (!this->hasQuest(refID))
	{
		// Quest is completely new
		// Can only add quest here atm
		std::string startquest = "StartQuest " + refID;
		std::string setstage = "SetStage " + refID + " " + StageId;
		g_consoleInterface->RunScriptLine(setstage.c_str(), nullptr);
		// TODO Tell server
	}
	else if (!this->hasStageUpdated(refID))
	{
		// Quest is already known, add stage
		//this->getcQuest(refID).addObjective(Objective);
		// Can only add new objective here
		std::string setstage = "SetStage " + refID + " " + StageId;
		// Updating a quest to the next stage usually makes it visible if it isn't already for some reason
		_MESSAGE("Setting stage for %s to %s", refID, StageId);
		g_consoleInterface->RunScriptLine2(setstage.c_str(), nullptr, false);
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

bool QuestManager::hasStageUpdated(std::string QuestID)
{
	//bool has_objective = false;
	//for (auto quest : AllQuests)
	//{
	//	if (int_to_hex_string(quest.getID()) == QuestID) // Find quest in list
	//	{
	//		if (quest.hasObjective(objectiveId))
	//		{
	//			has_objective = true;
	//		}
	//		break;
	//	}
	//}
	//return has_objective;
	return this->getcQuest(QuestID).HaveStagesChanged();
}

bool QuestManager::hasStageUpdated(UInt32 refID, UInt32 objectiveId)
{
	//bool has_objective = false;
	//for (auto quest : AllQuests)
	//{
	//	if (quest.getID() == refID) // Find quest in list
	//	{
	//		if (quest.hasObjective(std::to_string(objectiveId)))
	//		{
	//			has_objective = true;
	//		}
	//		break;
	//	}
	//}
	//return has_objective;
	return this->getcQuest(int_to_hex_string(refID)).HaveStagesChanged();
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

			break;
		}
		else
		{
			++active_quest_iterator; // Move to the next node, this one is still active
		}
	}
}

bool QuestManager::hasStageUpdated(BGSQuestObjective* Objective)
{
	//bool has_objective = false;
	//
	//for (auto quest : AllQuests)
	//{
	//	if (quest.getID() == Objective->quest->refID) // Find quest in list
	//	{
	//		if (quest.hasObjective(Objective))
	//		{
	//			has_objective = true;
	//		}
	//		break;
	//	}
	//}
	//return has_objective;
	return this->getcQuest(Objective).HaveStagesChanged();
}

//void QuestManager::removeActiveObjective(std::string refID, std::string objectiveId)
//{
//	auto active_objective_iterator = ActiveObjectives.begin();
//	while (active_objective_iterator != ActiveObjectives.end())
//	{
//		BGSQuestObjective* objective = (*active_objective_iterator);
//		cQuest Quest(objective);
//		if (int_to_hex_string(Quest.getID()) == refID && std::to_string(objective->objectiveId) == objectiveId) // If the completed flag is set
//		{
//
//			_MESSAGE("Removed stage %s, %s %s from the active objective list", std::to_string(objective->objectiveId), refID.c_str(), Quest.getName().c_str());
//			// Remove the objective from the list
//			ActiveObjectives.erase(active_objective_iterator++); // First increments the iterator with ++, then removes the previous node, as using ++ returns the node you are incrementing from
//			break;
//		}
//		else
//		{
//			++active_objective_iterator;
//		}
//	}
//}

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
	RetryConnection = true;
	ConnAckReceived = false;
	SyncedWithServer = false;
	SyncState = 0;
}

bool QuestManager::completedIntro()
{
	if (this->hasQuest("104c1c"))
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
			printQuestInfo(iterator->data, g_consoleInterface);
			QSyncMsgBody message_body;
			this->Add(iterator->data, &message_body);

			if (message_body.type != message_type::NONE)
			{
				message_for_server.addMessage(message_body);
			}

			iterator = iterator->next;
		}

		// Don't update objective count because that's already been done
		//client.send_message(message_for_server.toString());
	}

	//auto active_objective_iterator = ActiveObjectives.begin();
	//while (active_objective_iterator != ActiveObjectives.end())
	//{
	//	BGSQuestObjective* objective = (*active_objective_iterator);
	//	cQuest Quest(objective);
	//	if (Quest.isObjectiveCompleted(objective)) // If the completed flag is set
	//	{
	//		//UInt32 StageID = objective->objectiveId;
	//		//Quest.getStage();

	//		_MESSAGE("%s stage %i completed.", Quest.getName().c_str(), Quest.getStage());


	//		printQuestInfo(objective, g_consoleInterface);






	//		QSyncMsgBody msg;
	//		msg.type = message_type::OBJECTIVE_COMPLETED;
	//		json stage_info =
	//		{
	//			{"Stage", std::to_string(Quest.getStage())},
	//			{"ID", int_to_hex_string(Quest.getID())}
	//		};
	//		msg.body = stage_info;
	//		message_for_server.addMessage(msg);

	//		// Remove the objective from the list
	//		ActiveObjectives.erase(active_objective_iterator++); // First increments the iterator with ++, then removes the previous node, as using ++ returns the node you are incrementing from
	//	}
	//	else
	//	{
	//		++active_objective_iterator;
	//	}
	//}

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
					QSyncQuest quest_info = QSyncQuest(Quest.getIDString(), std::to_string(Quest.getStage()), Quest.GetStagesAsSimple(), Quest.isActive(), Quest.isComplete(), Quest.isFailed());
					json jsonQuest = {
						{"Quest", quest_info.ToString()}
					};
					msg.body = jsonQuest;
					message_for_server.addMessage(msg);
				}
				else
				{
					_MESSAGE("%s Failed!", Quest.getName().c_str());
					QSyncMsgBody msg;
					msg.type = message_type::QUEST_FAILED;
					QSyncQuest quest_info = QSyncQuest(Quest.getIDString(), std::to_string(Quest.getStage()), Quest.GetStagesAsSimple(), Quest.isActive(), Quest.isComplete(), Quest.isFailed());
					json jsonQuest = {
						{"Quest", quest_info.ToString()}
					};
					msg.body = jsonQuest;
					message_for_server.addMessage(msg);
				}
			}
			else
			{
				_MESSAGE("Quest '%s' '%s' Has been removed from the active list and is not completed or failed! %s", int_to_hex_string(Quest.getID()).c_str(), Quest.getName().c_str(), Quest.getFlagString().c_str());
				QSyncMsgBody msg;
				msg.type = message_type::QUEST_INACTIVE;
				QSyncQuest quest_info = QSyncQuest(Quest.getIDString(), std::to_string(Quest.getStage()), Quest.GetStagesAsSimple(), Quest.isActive(), Quest.isComplete(), Quest.isFailed());
				json jsonQuest = {
					{"Quest", quest_info.ToString()}
				};
				msg.body = jsonQuest;
				message_for_server.addMessage(msg);
			}
			// Remove this quest from the list
			ActiveQuests.erase(active_quest_iterator++); // First increments the iterator with ++, then removes the previous node as using ++ returns the node you are incrementing from
		}
		//else if (Quest.HaveStagesChanged())
		//{

		//}
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
		_MESSAGE("%s", message);
		if (message.size() == 0)
		{
			// Server has closed the connection
			_MESSAGE("Server has closed the socket!");
			Console_Print("Disconnected from Quest Sync Server");
			std::string message = "MessageBoxEx \"Disconnected from Quest Sync Server! Will attempt to reconnect.\"";
			g_consoleInterface->RunScriptLine(message.c_str(), nullptr);
			client->Disconnect();
			this->reset();
			break;
		}
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
				instruction["Body"]["message"].get_to(msg_from_server);
				_MESSAGE("Message From Server: %s", msg_from_server.c_str());
				QSyncVersion versionInfo;
				versionInfo.fromString(msg_from_server);
				bool serverResult = versionInfo.isVersionCompatible(ClientVersion::Version[0], ClientVersion::Version[1]);
				// Determine if allowed to connect
				if (serverResult)
				{
					ConnAckReceived = true;
					_MESSAGE("Server version is compatible, maintaining connection.");
					Console_Print("Server version is compatible, maintaining connection.");
					std::string message = "MessageEx \"Quest Sync plugin is ready.\"";
					g_consoleInterface->RunScriptLine(message.c_str(), nullptr);
				}
				else
				{
					_MESSAGE("Server version is incompatible with this version of the plugin, disconnecting and not retrying. Update plugin version to solve.");
					Console_Print("Server version incompatible, disconnecting from Quest Sync Server.");
					std::string message = "MessageBoxEx \"Quest Sync plugin is outdated! Disconnecting from the server.\"";
					g_consoleInterface->RunScriptLine(message.c_str(), nullptr);
					client->Disconnect();
					RetryConnection = false;
				}
			}
			break;
			case message_type::START_QUEST:
			{
				_MESSAGE("Server wants me to start a new quest:");
				auto quest_json = instruction["Body"];
				std::string questString;
				quest_json["Quest"].get_to(questString);
				QSyncQuest qQuest(questString);

				_MESSAGE("%s Stage: %s", qQuest.GetRefId().c_str(), qQuest.GetStageId().c_str());
				this->Add(qQuest.GetRefId(), qQuest.GetStageId(), g_consoleInterface); // Add through console
				this->Add(questObjectiveList.Head()->data, nullptr); // Add the newly aquired quest here (does nothing if quest already exists)
			}
			break;
			case message_type::UPDATE_QUEST:
			{
				_MESSAGE("Server wants me to progress a quest:");
				auto quest_json = instruction["Body"];
				std::string questString;
				quest_json["Quest"].get_to(questString);
				QSyncQuest qQuest(questString);

				_MESSAGE("%s Stage: %s", qQuest.GetRefId().c_str(), qQuest.GetStageId().c_str());
				this->Add(qQuest.GetRefId(), qQuest.GetStageId(), g_consoleInterface); // Add through console
				this->Add(questObjectiveList.Head()->data, nullptr); // Add the newly aquired quest here (does nothing if quest already exists)
			}
			break;
			case message_type::COMPLETE_OBJECTIVE:
			{
				// TODO: Remove this code maybe, shouldn't be requied
				//_MESSAGE("Server has told me to complete a quest objective");
				//auto stage_info = instruction["Body"];
				////SetObjectiveCompleted Quest:baseform objectiveIndex:int completedFlag:int{0/1}
				//std::string ID;
				//stage_info["ID"].get_to(ID);
				//std::string Stage;
				//stage_info["Stage"].get_to(Stage);
				//_MESSAGE("%s %s", ID.c_str(), Stage.c_str());
				//std::string completeStage = "SetObjectiveCompleted " + ID + " " + Stage + " 1";
				//g_consoleInterface->RunScriptLine2(completeStage.c_str(), nullptr, false);
				//this->removeActiveObjective(ID, Stage);
				_MESSAGE("Server sent a message using COMPLETE_OBJECTIVE, this is removed.");

			}

			break;
			case message_type::COMPLETE_QUEST:
			{
				_MESSAGE("Server wants me to complete quest:");
				auto quest_json = instruction["Body"];
				std::string questString;
				quest_json["Quest"].get_to(questString);
				QSyncQuest qQuest(questString);

				_MESSAGE("%s %s", qQuest.GetRefId().c_str(), qQuest.GetStageId().c_str());
				std::string complete = "CompleteQuest " + qQuest.GetRefId();
				g_consoleInterface->RunScriptLine(complete.c_str(), nullptr);
				this->removeActiveQuest(qQuest.GetRefId());
				// TODO figure out how much XP should be earned
			}
			break;
			case message_type::FAIL_QUEST:
			{
				_MESSAGE("Server wants me to fail quest:");
				auto quest_json = instruction["Body"];
				std::string questString;
				quest_json["Quest"].get_to(questString);
				QSyncQuest qQuest(questString);

				_MESSAGE("%s %s", qQuest.GetRefId().c_str(), qQuest.GetStageId().c_str());
				std::string fail = "FailQuest " + qQuest.GetRefId();
				g_consoleInterface->RunScriptLine(fail.c_str(), nullptr);
				this->removeActiveQuest(qQuest.GetRefId());
			}
			break;
			case message_type::CURRENT_ACTIVE_QUESTS:
			{
				// Server has sent me a list of all quests and objectives that I should have active
				_MESSAGE("Sever has replied with current active quests and objectives");
				json quest_list = instruction["Body"];

				_MESSAGE("Expecting a crash here...");
				//quest_list.get_to(stage_list);
				_MESSAGE("%s", quest_list.dump().c_str());
				_MESSAGE("Going to loop through the list of quests and their objectives");
				for (auto quest : quest_list)
				{
					//_MESSAGE(quest_s.c_str());
					_MESSAGE("Or here???");
					//auto quest = json::parse(quest_s);
					std::string questString;
					quest["Quest"].get_to(questString);
					QSyncQuest qQuest(questString);
					_MESSAGE("Quest parsed...");
					

					_MESSAGE("Extracted info, checking against the current lists...");
					// Let Add decide what to do with this information
					for (auto stage : qQuest.GetStages())
					{
						if (stage.Completed == true)
						{
							this->Add(qQuest.GetRefId(), qQuest.GetStageId(), g_consoleInterface); // Add through console
							this->Add(questObjectiveList.Head()->data, nullptr); // Add the newly aquired quest here (does nothing if quest already exists)
						}
					}
				}
				// Create a list of current quests and objectives
				//json current_quests = json::array();
				//for (auto objective : ActiveObjectives)
				//{
				//	std::string quest_name = objective->quest->GetFullName() ? objective->quest->GetFullName()->name.CStr() : "<no name>";
				//	json quest_info =
				//	{
				//		{"ID", int_to_hex_string(objective->quest->refID)},
				//		{"Stage", std::to_string(objective->objectiveId)},
				//		{"Name", quest_name},
				//		//{"Flags", flagString}
				//	};
				//	_MESSAGE("Telling the server about %s stage %s", quest_name.c_str(), std::to_string(objective->objectiveId).c_str());
				//	current_quests.push_back(quest_info);
				//}
				json current_quests = json::array();
				for (auto quest : ActiveQuests)
				{
					cQuest cquest(quest);
					QSyncQuest quest_info = QSyncQuest(cquest.getIDString(), std::to_string(cquest.getStage()), cquest.GetStagesAsSimple());
					json jsonQuest = {
						{"Quest", quest_info.ToString()}
					};

					current_quests.push_back(jsonQuest);
				}
				_MESSAGE("Telling the server about %d quests", current_quests.size());
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


	if (message_for_server.getSize() > 0 && client->isConnected()) // If there's any messaged to send to the server, send em!
	{
		std::string a_test = message_for_server.toString();
		_MESSAGE("Sending:\n%s", a_test);
		client->send_message(a_test);
	}
}

bool QuestManager::retryConnection()
{
	return RetryConnection;
}

cQuest::cQuest(TESQuest* Quest)
{
	setQuest(Quest);
}

cQuest::cQuest(BGSQuestObjective* Objective)
{
	setQuest(Objective->quest);
	//addObjective(Objective);
}

/// <summary>
/// Gets the quest ID as a UInt32
/// </summary>
/// <returns></returns>
UInt32 cQuest::getID()
{
	return Quest->refID;
}

//std::vector<BGSQuestObjective*> cQuest::getObjectives()
//{
//	return Objectives;
//}

//bool cQuest::hasObjective(std::string objectiveId)
//{
//	bool has_objective = false;
//	for (auto objective : Objectives) // Does it have the objective?
//	{
//		if (objective->objectiveId == stoi(objectiveId))
//		{
//			has_objective = true;
//			break;
//		}
//	}
//	return has_objective;
//}

//bool cQuest::hasObjective(BGSQuestObjective* Objective)
//{
//	bool has_objective = false;
//	for (auto objective : Objectives) // Does it have the objective?
//	{
//		if (objective->objectiveId == Objective->objectiveId)
//		{
//			has_objective = true;
//			break;
//		}
//	}
//	return has_objective;
//}

/// <summary>
/// Get quest flags as a list of booleans
/// </summary>
/// <returns></returns>
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

//void cQuest::addObjective(BGSQuestObjective* Objective)
//{
//	Objectives.push_back(Objective);
//}

/// <summary>
/// Get the full name of the quest.
/// </summary>
/// <returns></returns>
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
/// <summary>
/// Init the cQuest object
/// </summary>
/// <param name="Quest"></param>
void cQuest::setQuest(TESQuest* Quest)
{
	this->Quest = Quest;
	this->PreviousStages.clear();

	std::vector<TESQuest::StageInfo*> stageInfo = this->getStages();

	for (int i = 0; i < stageInfo.size(); i++)
	{
		stage newStage = {
			stageInfo[i]->stage,
			stageInfo[i]->isDone
		};
		this->PreviousStages.push_back(newStage);
	}

}
/// <summary>
/// Get the quest ID as a hex string, this is the FormID
/// </summary>
/// <returns>FormID for quest</returns>
std::string cQuest::getIDString()
{
	std::stringstream stream;
	int number = this->getID();
	stream << std::hex << number;
	return std::string(stream.str());
}

/// <summary>
/// Gets the current stage for a quest
/// </summary>
/// <returns>Current stage ID</returns>
UInt8 cQuest::getStage()
{
	return this->Quest->currentStage;
}

/// <summary>
/// Gets all stages for a quest
/// </summary>
/// <returns>All quest stages and if completed</returns>
std::vector<TESQuest::StageInfo*> cQuest::getStages()
{
	std::vector<TESQuest::StageInfo*> result;
	auto ittr = Quest->stages.Begin();
	while (ittr != Quest->stages.end())
	{
		result.push_back(ittr.Get());
		ittr.Next();
	}
	return result;
}

std::vector<stage> cQuest::GetStagesAsSimple()
{
	std::vector<stage> stages;
	stage newStage = stage();
	std::vector<TESQuest::StageInfo*> stageInfo = this->getStages();

	for (int i = 0; i < stageInfo.size(); i++)
	{
		newStage.ID = stageInfo[i]->stage;
		newStage.Completed = stageInfo[i]->isDone;
		stages.push_back(newStage);
	}

	return stages;
}

/// <summary>
/// Checks to see if the quest has updated since the last time this function was called.
/// </summary>
/// <returns>If any stages have changed</returns>
bool cQuest::HaveStagesChanged()
{
	bool result = false;
	std::vector<TESQuest::StageInfo*> currentStages = this->getStages();

	for (int i = 0; i < PreviousStages.size(); i++)
	{
		if (PreviousStages[i].Completed != currentStages[i]->isDone)
		{
			result = true;
			// Update to remember new stage
			PreviousStages[i].Completed = currentStages[i]->isDone;
			// Don't break because more than one stage could potentially be updated and doing a second loop to update the state is silly
		}
	}

	return result;
}

std::string cQuest::getStagesString()
{
	std::vector<TESQuest::StageInfo*> stages = this->getStages();
	std::string result = "";
	for (TESQuest::StageInfo* stage : stages)
	{
		result += "Stage:\t\t" + std::to_string(stage->stage);
		result += "\tIsDone:\t" + std::to_string(stage->isDone);
		result += "\n";
	}
	return result;
}





// Stolen from JIP. Like straight up theft, no asking.


std::string cQuest::getObjectivesString(NVSEConsoleInterface* g_consoleInterface)
{

	std::string result = "";

		

	result += "Objectives?\n";

		//UInt32 completed = 1;
		////TempElements* tmpElements = GetTempElements();
		//auto iter = Quest->lVarOrObjectives.Head();
		//if (completed) completed = 2;
		//completed++;
		//do
		//{
		//	if (BGSQuestObjective* objective = (BGSQuestObjective*)iter->data; objective && IS_TYPE(objective, BGSQuestObjective) && (completed == (objective->status & 3)))
		//		//tmpElements->Append((int)objective->objectiveId);
		//	result += (int)objective->objectiveId;
		//} while (iter = iter->next);
		/*std::stringstream stream;
		* 
		int number = this->getID();
		stream << std::hex << number;
		return std::string(stream.str());*/

		
		

		
		//result += tmpElements->Data()->String();
		result += "\n";

		TESObjectREFR* test = TESObjectREFR::Create(true);
		std::string command = "GetQuestObjectives ";
		command += getIDString();
		command += " 1";
		g_consoleInterface->RunScriptLine(command.c_str(), test);


	return result;
}

std::vector<VariableInfo*> cQuest::getQuestVariables()
{
	std::vector<VariableInfo*> variables;
	auto ittr = Quest->lVarOrObjectives.Begin();
	while (ittr != Quest->lVarOrObjectives.end())
	{
		variables.push_back(ittr->varInfoIndex);
		_MESSAGE("?: %lu", ittr->varInfoIndex->idx);
		_MESSAGE("??: %llu", ittr->varInfoIndex->idx);
		_MESSAGE("???: %s", ittr->varInfoIndex->name);
		ittr.Next();
	}

	return variables;
}


std::string cQuest::getVariblesString()
{
	std::vector<VariableInfo*> variables = this->getQuestVariables();
	std::string result = "";
	for (auto variable : variables)
	{
		result += "ID: " + std::to_string(variable->idx);
		result += "\n\tName: ";
		//result += variable->name.CStr();
		result += variable->GetTESForm()->GetFullName()->name.CStr();
		result += "\n\ttype: " + std::to_string(variable->type);
		result += "\n\tdata: " + std::to_string(variable->data);
		result += "\n";
	}

	return result;
}

void printQuestInfo(BGSQuestObjective* Quest, NVSEConsoleInterface* g_consoleInterface)
{
	cQuest cquest = cQuest(Quest);
	_MESSAGE("Quest ID: %s", cquest.getIDString().c_str());
	_MESSAGE("Name: %s", cquest.getName().c_str());
	_MESSAGE("Flags: %s", cquest.getFlagString().c_str());
	_MESSAGE("Stage: %d", cquest.getStage());
	_MESSAGE("Stages:");
	_MESSAGE("%s", cquest.getStagesString().c_str());
	/*_MESSAGE("Objectives");
	_MESSAGE("%s", cquest.getObjectivesString(g_consoleInterface).c_str());*/
	/*_MESSAGE("Variables:");
	_MESSAGE("%s", cquest.getVariblesString().c_str());*/
}

