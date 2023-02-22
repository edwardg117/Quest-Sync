#include "nvse/PluginAPI.h"
#include "nvse/CommandTable.h"
#include "nvse/GameAPI.h"
#include "nvse/ParamInfos.h"
#include "nvse/GameObjects.h"
#include <string>
#include <iostream>
#include "nvse/utility.h"
//NoGore is unsupported in xNVSE

#include "TCPClient.h"
#include "filthy_ini.h"
#include "nvse/GameForms.h"
#include "Quest Sync Server/QsyncDefinitions.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

IDebugLog		gLog("Quest_Sync.log");
PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

NVSEMessagingInterface* g_messagingInterface{};
NVSEInterface* g_nvseInterface{};
NVSECommandTableInterface* g_cmdTableInterface{};

// RUNTIME = Is not being compiled as a GECK plugin.
#if RUNTIME
NVSEScriptInterface* g_script{};
NVSEStringVarInterface* g_stringInterface{};
NVSEArrayVarInterface* g_arrayInterface{};
NVSEDataInterface* g_dataInterface{};
NVSESerializationInterface* g_serializationInterface{};
NVSEConsoleInterface* g_consoleInterface{};
NVSEEventManagerInterface* g_eventInterface{};
bool (*ExtractArgsEx)(COMMAND_ARGS_EX, ...);
TCPClient client("", 0);
#endif

/****************
 * Here we include the code + definitions for our script functions,
 * which are packed in header files to avoid lengthening this file.
 * Notice that these files don't require #include statements for globals/macros like ExtractArgsEx.
 * This is because the "fn_.h" files are only used here,
 * and they are included after such globals/macros have been defined.
 ***************/
#include "fn_intro_to_script_functions.h" 
#include "fn_typed_functions.h"


// Shortcut macro to register a script command (assigning it an Opcode).
#define RegisterScriptCommand(name) 	nvse->RegisterCommand(&kCommandInfo_ ##name)

// Short version of RegisterScriptCommand.
#define REG_CMD(name) RegisterScriptCommand(name)

// Use this when the function's return type is not a number (when registering array/form/string functions).
//Credits: taken from JohnnyGuitarNVSE.
#define REG_TYPED_CMD(name, type)	nvse->RegisterTypedCommand(&kCommandInfo_##name,kRetnType_##type)

std::string int_to_hex_string(int number);
UInt32 g_previousQuestCount = 0;
struct q_qsync_states
{
	bool conn_ack_received = false;
	bool game_loaded = false;

};
struct q_qsync_states g_qsync_states;
std::list<TESQuest*> g_current_quest_list;
enum class EFlagValue
{
	/*Flag1 = 1 << 0, // 1
	Flag2 = 1 << 1, // 2
	Flag3 = 1 << 2, // 4
	Flag4 = 1 << 3, // 8
	Flag5 = 1 << 4, // 16
	Flag6 = 1 << 5, // 32
	Flag7 = 1 << 6, // 64
	Flag8 = 1 << 7  //128*/
	Flag1 = 1,
	Flag2 = 2,
	Flag3 = 4,
	Flag4 = 8,
	Flag5 = 16,
	Flag6 = 32,
	Flag7 = 64,
	Flag8 = 128
};
struct BitFlag
{
	uint8_t m_FlagValue = 0;
};
// Figure out what bits are set
std::vector<bool> get_quest_flag_states(TESQuest* quest)
{
	std::vector<bool> flag_states; // A vector of states for the quest flags
	
	// Perform a comparison using the AND operator to compare the bits
	// 0000 0000 & 0000 0001 = false
	// 0000 0001 & 0000 0001 = true
	// 0010 0011 & 0000 0001 = true

	flag_states.push_back((quest->flags & 1) == 1);		// Bit 1 - Active
	flag_states.push_back((quest->flags & 2) == 2);		// Bit 2 - Completed
	flag_states.push_back((quest->flags & 4) == 4);		// Bit 3
	flag_states.push_back((quest->flags & 8) == 8);		// Bit 4
	flag_states.push_back((quest->flags & 16) == 16);	// Bit 5
	flag_states.push_back((quest->flags & 32) == 32);	// Bit 6 - Visible in pip-boy
	flag_states.push_back((quest->flags & 64) == 64);	// Bit 7 - Failed
	flag_states.push_back((quest->flags & 128) == 128);	// Bit 8

	return flag_states;
}

// Returns if the "Active" bit is set or not
bool is_quest_active(TESQuest* quest)
{
	return (quest->flags & 1) == 1;
}
// Returns if the "Completed" bit is set
bool is_quest_complete(TESQuest* quest)
{
	return (quest->flags & 2) == 2;
}
// Returns if the quest was failed
bool is_quest_failed(TESQuest* quest)
{
	return (quest->flags & 64) == 64;
}
//TCPClient client("", 0);
// This is a message handler for nvse events
// With this, plugins can listen to messages such as whenever the game loads
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case NVSEMessagingInterface::kMessage_PostLoad:
		_MESSAGE("Post Load - plugintest running");
		//TCPClient client("127.0.0.1", 25575);

		break;
	case NVSEMessagingInterface::kMessage_ExitGame:
		_MESSAGE("Exit Game - plugintest running");
		client.Cleanup();
		break;
	case NVSEMessagingInterface::kMessage_ExitToMainMenu:
		_MESSAGE("Exit to main menu - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_LoadGame:
		_MESSAGE("Load game - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_SaveGame:
		_MESSAGE("Save game - plugintest running");
		break;
#if EDITOR
	case NVSEMessagingInterface::kMessage_ScriptEditorPrecompile: break;
#endif
	case NVSEMessagingInterface::kMessage_PreLoadGame:
		_MESSAGE("Pre load game - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_ExitGame_Console:
		_MESSAGE("Exit Game - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_PostLoadGame:
		_MESSAGE("Post Load Game - plugintest running");
		g_qsync_states.game_loaded = true;
		break;
	case NVSEMessagingInterface::kMessage_PostPostLoad:
		_MESSAGE("Post Post Load - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_RuntimeScriptError: break;
	case NVSEMessagingInterface::kMessage_DeleteGame: break;
	case NVSEMessagingInterface::kMessage_RenameGame: break;
	case NVSEMessagingInterface::kMessage_RenameNewGame: break;
	case NVSEMessagingInterface::kMessage_NewGame:
		_MESSAGE("New Game - plugintest running");
		g_qsync_states.game_loaded = true;
		break;
	case NVSEMessagingInterface::kMessage_DeleteGameName:
		_MESSAGE("Delete Game Name - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_RenameGameName: break;
	case NVSEMessagingInterface::kMessage_RenameNewGameName: break;
	case NVSEMessagingInterface::kMessage_DeferredInit:
		_MESSAGE("DefferedInit - plugintest running");
		/* {std::string gamePath = GetFalloutDirectory();
		std::cout << "From the get direcroty thing: " << gamePath;
		std::cout << "From the Interface: " << g_nvseInterface->GetRuntimeDirectory(); }*/
		{
			std::map<std::string, std::string> settings = filthy_ini::GetIp(g_nvseInterface, "quest_sync.ini");
			client.SetServerIp(settings["IpAddress"]);
			client.SetServerPort(stoi(settings["Port"]));
		}
		client.init();
		client.Connect();
		if (client.isConnected())
		{
			_MESSAGE("Conection Success!");
		}
		else
		{
			_MESSAGE("Unable to connect :(");
			std::cout << WSAGetLastError() << std::endl;
		}
		break;
	case NVSEMessagingInterface::kMessage_ClearScriptDataCache: break;
	case NVSEMessagingInterface::kMessage_MainGameLoop:
		//Console_Print("Main Game Loop - plugintest running");
		// {} to change scope
		// 1. Get messages from server and apply actions
		// 2. Do I look for quest updates?
		//	2.1 Has the list of quests increased in count?
		//		2.1.1 Get latest addition/s
		//		2.1.2 Is this a new quest or an existing one that's now in progress? (flag=2)
		//			If so add to current quest list
		// 
		// 3. Check for quest completions/failures? -- Create list of quests that are in progress
		//	3.1 Check to see if the flag is no longer '2'
		//	3.1.1 If flag is 4 - Quest is complete
		//	3.1.2 If flag is something else - Quest failed?

		// 1. Get messages from server and apply actions
	{std::list<std::string> messages = client.GetMessages();
	for (auto message : messages)
	{
		//std::cout << "Received message, parsing!";
		_MESSAGE("Received message, parsing!");
		//auto parsed_message = json::parse(message);
		//message_type m_type;
		//parsed_message["message_type"].get_to(m_type);
		QSyncMessage msg(message);
		//msg.fromString(message);
		// Switch message types from the server, full list in QsyncDefinitions
		if (!g_qsync_states.conn_ack_received || !g_qsync_states.game_loaded)
		{
			if (msg.type != message_type::CONNECTION_ACKNOWLEDGEMENT) { _MESSAGE("Ignored message from server as the game save hasn't loaded yet.");  break; } // Don't do anything other than a connection acknowledgement if these are true
		}
		switch (msg.type)
		{
		case message_type::CONNECTION_ACKNOWLEDGEMENT:
		{
			//std::string message_contents;
			//parsed_message["message_contents"].get_to(message_contents);
			_MESSAGE("Server sent Connection Acknowledgement");
			_MESSAGE("Message From Server: %s", msg.body.c_str());
			g_qsync_states.conn_ack_received = true;

			//g_dataInterface->
			//_MESSAGE("Citing current player quests...");
			//std::cout << "Current Player Quests: " << PlayerCharacter::GetSingleton()->questObjectiveList.Count() << std::endl;
			//json message = {
			//	{"message_type", message_type::RESEND_CONN_ACK},
			//	{"message_contents", 10}
			//};
			//client.send_message(message.dump());

		}
		break;
		case message_type::START_QUEST: 
		{
			_MESSAGE("Server wants me to start a new quest:");
			auto quest_info = json::parse(msg.body);
			//auto quest_info = json::parse(message.body);
			//_MESSAGE(msg.body.c_str());
			std::string ID;
			quest_info["ID"].get_to(ID);
			std::string Name;
			quest_info["Name"].get_to(Name);
			std::string Stage;
			quest_info["Stage"].get_to(Stage);
			std::string Flags;
			quest_info["Flags"].get_to(Flags);
			_MESSAGE("%s %s %s %s", ID.c_str(), Name.c_str(), Stage.c_str(), Flags.c_str());
			std::string startquest = "StartQuest " + ID;
			std::string setstage = "SetStage " + ID + " " + Stage;
			//setstage += std::string(" ");
			//setstage += quest_info["Stage"];
			g_consoleInterface->RunScriptLine(setstage.c_str(), nullptr);
		}
			break;
		case message_type::UPDATE_QUEST: 
		{
			_MESSAGE("Server wants me to progress a quest:");
			auto quest_info = json::parse(msg.body);
			//SetObjectiveCompleted Quest:baseform objectiveIndex:int completedFlag:int{0/1}
			std::string ID;
			quest_info["ID"].get_to(ID);
			std::string Name;
			quest_info["Name"].get_to(Name);
			std::string Stage;
			quest_info["Stage"].get_to(Stage);
			std::string Flags;
			quest_info["Flags"].get_to(Flags);
			_MESSAGE("%s %s %s %s", ID.c_str(), Name.c_str(), Stage.c_str(), Flags.c_str());
			std::string setstage = "SetStage " + ID + " " + Stage;
			//setstage += std::string(" ");
			//setstage += quest_info["Stage"];
			g_consoleInterface->RunScriptLine(setstage.c_str(), nullptr);
		}
		break;
		case message_type::COMPLETE_QUEST: 
		{
			_MESSAGE("Server wants me to complete quest:");
			auto quest_info = json::parse(msg.body);
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
		}
		break;
		case message_type::FAIL_QUEST:
		{
			_MESSAGE("Server wants me to fail quest:");
			auto quest_info = json::parse(msg.body);
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
		}
		break;
		case message_type::CURRENT_QUESTS_COMPLETEION: break;
		case message_type::ALL_QUEST_STATES: break;
		case message_type::RESEND_CONN_ACK:
		{
			//_MESSAGE("Citing current player quests 2...");
			PlayerCharacter* player = PlayerCharacter::GetSingleton();
			std::cout << "Current Player Quests: " << player->questObjectiveList.Count() << std::endl;
			if (player->questObjectiveList.Count())
			{
				TESQuest* qwest;
				//std::vector<TESQuest*> quests;
				int i = 0;
				auto iterator = player->questObjectiveList.Head();
				do
				{
					/*iterator->data->objectiveId;

					iterator->data->quest;
					iterator->data->quest->currentStage;
					iterator->data->quest->editorName;
					iterator->data->quest->flags;
					iterator->data->quest->fullName;

					iterator->data->status;
					iterator->data->targets;
					iterator->data->eQObjStatus_completed;*/



					//std::cout << iterator->data->quest->GetStringRepresentation() << std::endl;
					// Convert refID to hex string
					//std::stringstream stream;
					//stream << std::hex << iterator->data->quest->refID;
					//std::string refID_hex(stream.str());

					//std::cout << int_to_hex_string(iterator->data->quest->currentStage) << " || " << int_to_hex_string(iterator->data->quest->refID) << " || " << int_to_hex_string(iterator->data->status) << " || " << int_to_hex_string(iterator->data->quest->flags) << std::endl;
					//iterator->data->quest->
					//if (int_to_hex_string(iterator->data->quest->refID) == "10a214")
						if(true)
					{
						qwest = iterator->data->quest;
						std::cout << qwest->GetStringRepresentation() << std::endl;
						//std::cout << "|Stage| " << std::to_string(qwest->currentStage) << "\n|ID| " << int_to_hex_string(qwest->refID) << "\n|Flag| " << (1 << qwest->flags) << "\n|Repeats| " << std::endl;
						//i++;
						//int i = 0, j = 0, k = 0;
						//std::cout << iterator->data->quest->GetStringRepresentation() << std::endl;
						//std::cout << "|Stage| " << std::to_string(iterator->data->quest->currentStage) << " |ID| " << int_to_hex_string(iterator->data->quest->refID) << " |Flag| " << (1 << iterator->data->quest->flags) << std::endl;
						//std::cout << "Stage Count: " << iterator->data->quest->stages.Count() << std::endl;
						/*auto quest_itr = iterator->data->quest->stages.Head();
						do
						{
							//std::cout << "StageData: " << quest_itr->data->stage << std::endl;
							//if (quest_itr->data->stage == iterator->data->quest->currentStage) { std::cout << "Quest matches stage found at: " << i << std::endl; }
							//std::cout << "Status: " << quest_itr->data->unk001 << std::endl;
							//if (quest_itr->data->unk001 == true) { j++; }
							//else { k++; }
							//i++;
						} while (quest_itr = quest_itr->next);
						std::cout << "Stages Complete:   " << j << std::endl;
						std::cout << "Stages Incomplete: " << k << std::endl;*/
					}
				} while (iterator = iterator->next);

				/**
				* Quest ends up added to the questObjectiveList every time it advances to a new stage but not when it gets completed (see repeats below)
				* Quest stage and flag updates for all entries in the list, so only need to worry about looking at info about the quest once. All others can be ignored.
				*/
				//std::cout << qwest->GetStringRepresentation() << std::endl;
				//std::cout << "|Stage| " << std::to_string(qwest->currentStage) << "\n|ID| " << int_to_hex_string(qwest->refID) << "\n|Flag| " << (1 << qwest->flags) << "\n|Repeats| " << i << std::endl;
			}


			//_MESSAGE("Building reply to server.");
			json json_message = {
				{"message_type", message_type::RESEND_CONN_ACK},
				{"message_contents", 10}
			};
			//_MESSAGE("Sending reply to server.");
			client.send_message(json_message.dump());
		}
		break;
		default:
			_MESSAGE("Message Structure invalid!");

		}
	}}

	if (!g_qsync_states.conn_ack_received || !g_qsync_states.game_loaded) { break; } // Don't do any of this if the game isn't ready

	// 2. look for quest updates
	{
		//	2.1 Has the list of quests increased in count?
		//_MESSAGE("Finished Tasks from the server, doing my own.");
		PlayerCharacter* player = PlayerCharacter::GetSingleton();
		if (player->questObjectiveList.Count() > g_previousQuestCount)
		{
			// There is a new quest started or a quest has been updated
			// 2.1.1 Get latest addition/s
			UInt32 num_new_quests = player->questObjectiveList.Count() - g_previousQuestCount; // Unint because that's what .Count() returns, also never a negative number so why bother mnaking it signed?
			auto iterator = player->questObjectiveList.Head(); // This is what I'll use to iterate through the list (sort of)
			_MESSAGE("There's %i new entries in the pip-boy", num_new_quests);

			//std::vector<TESQuest*> new_quest_list;

			for (int current_new_quest = 0; current_new_quest < num_new_quests; current_new_quest++) // Do only new entries in the pip-boy
			//for (int current_new_quest = 0; current_new_quest < player->questObjectiveList.Count(); current_new_quest++) // Do ALL quest entries in the pip-boy
			{
				//2.1.2 Is this a new quest or an existing one that's now in progress? 
				TESQuest* new_quest = iterator->data->quest; // The quest attached to the current entry
				UInt32 stage = iterator->data->objectiveId; // The quest stage as it appears on the fallout wiki
				// Is this a new quest or an entry for an existing one?
				bool is_actually_new = true;
				for (auto existing_quest : g_current_quest_list)
				//for(auto existing_quest : new_quest_list)
				{
					if (existing_quest->refID == new_quest->refID) { is_actually_new = false; break; } // If reference ids match, it's the same! (duh)
				}
				// Create a pretty string to represent the flags "10001100"
				std::string flagString = "";
				std::vector<bool> quest_flags = get_quest_flag_states(new_quest);
				for (auto flag : quest_flags)
				{
					flagString += flag ? "1" : "0";
				}
				// Get the quest name as displayed in the pip-boy
				std::string quest_name = new_quest->GetFullName() ? new_quest->GetFullName()->name.CStr() : "<no name>";

				if (is_actually_new && is_quest_active(new_quest) && !is_quest_complete(new_quest)) // Quest can remain active when complete
				{ 
					// Quest was just added to the pip-boy
					g_current_quest_list.push_back(new_quest); // Add to the list fo quests I'm monitoring for completeion
					//new_quest_list.push_back(new_quest);

					_MESSAGE("New Quest '%s' '%s' stage: %s added! %s", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), std::to_string(stage).c_str(), flagString.c_str());
					//TODO Tell the server about this new quest
					json quest_info =
					{
						{"ID", int_to_hex_string(new_quest->refID)},
						{"Stage", std::to_string(stage)},
						{"Name", quest_name},
						{"Flags", flagString}
					};

					/*_MESSAGE("Stages:");
					tList<TESQuest::StageInfo> stages = new_quest->stages;
					auto stage = stages.Head();
					do
					{
						UInt8 stageID = stage->data->stage;
						UInt8 complete = stage->data->unk001;
						//auto test = unsigned(stageID);
						_MESSAGE("%i: %i", unsigned(stageID), unsigned(complete));
					} while (stage = stage->next);

					//new_quest->GetOb
					_MESSAGE("objId: %i", unsigned(iterator->data->objectiveId));
					_MESSAGE(iterator->data->displayText.CStr());
					_MESSAGE ("%i", unsigned(iterator->data->status));
					//iterator->data->targets;*/


					QSyncMessage msg(message_type::NEW_QUEST, quest_info.dump());
					client.send_message(msg.toString());
				}
				else if (is_quest_active(new_quest))
				{
					// Quest already exists in the pip-boy and has only updated
					_MESSAGE("Quest '%s' '%s' updated to stage: %s ! %s", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), std::to_string(stage).c_str(), flagString.c_str());
					// TODO Tell server that this quest updated
					json quest_info =
					{
						{"ID", int_to_hex_string(new_quest->refID)},
						{"Stage", std::to_string(stage)},
						{"Name", quest_name},
						{"Flags", flagString}
					};
					/*_MESSAGE("Stages:");
					tList<TESQuest::StageInfo> stages = new_quest->stages;
					auto stage = stages.Head();
					do
					{
						UInt8 stageID = stage->data->stage;
						UInt8 complete = stage->data->unk001;
						//auto test = unsigned(stageID);
						_MESSAGE("%i: %i", unsigned(stageID), unsigned(complete));
					} while (stage = stage->next);

					_MESSAGE("objId: %i", unsigned(iterator->data->objectiveId));
					_MESSAGE(iterator->data->displayText.CStr());
					_MESSAGE("%i", unsigned(iterator->data->status));*/

					QSyncMessage msg(message_type::QUEST_UPDATED, quest_info.dump());
					client.send_message(msg.toString());
				}
				/*else if (is_actually_new) // Add ALL quests to this list for testing purposes
				{
					new_quest_list.push_back(new_quest);
				}*/
				else
				{
					//_MESSAGE("!!!Sanity check: The following quest was just added to the quest list but is inactive.!!!");
					_MESSAGE("Quest '%s' '%s' stage: %s has appeared but is not active! %s This may not be an issue.", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), std::to_string(stage).c_str(), flagString.c_str());
					// Most likely this quest will be picked activated when it is next updated and I can add it then
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

			// Now that I have unique quests only I can print out all the info I want on them
			/*std::cout << "Vector size: " << new_quest_list.size() << std::endl;
			for (auto quest : new_quest_list)
			{
				std::string flagString = "";
				std::vector<bool> quest_flags = get_quest_flag_states(quest);
				for (auto flag : quest_flags)
				{
					flagString += flag ? "1" : "0";
				}
				std::cout << "'" << int_to_hex_string(quest->refID) << "' '" << (quest->GetFullName() ? quest->GetFullName()->name.CStr() : "<no name>") << " " << flagString << std::endl;
				//std::vector<bool> quest_flags = get_quest_flag_states(quest);
				//std::cout << "Flags:" << std::endl;

				//std::vector<std::string> flag_guesses = { "Active", "Completed", "Bit 3", "Bit 4", "Bit 5", "bit 6", "bit 7", "bit 8" };
				//for (int flag_num = 0; flag_num < quest_flags.size(); flag_num++)
				//{
				//	std::cout << flag_num << ": " << quest_flags[flag_num] << " - " << flag_guesses[flag_num] << std::endl;
				//	//_MESSAGE("%i: %i", flag_num, quest_flags[flag_num]);
				//}
			}*/

			g_previousQuestCount += num_new_quests; // Update the number of quest objectives that were in the list on this run so I can compare to it next run
			
			//g_consoleInterface->RunScriptLine("GetQuestCompleted 10a214", nullptr);
		}

		// 3. Check for quest completions/failures? -- Create list of quests that are in progress
		//	3.1 Check to see if the flag is no longer "Active"
		if (g_current_quest_list.size()) // TODO check if it crashes when I remove this
		{
			auto active_quest_iterator = g_current_quest_list.begin();
			while (active_quest_iterator != g_current_quest_list.end())
			{
				TESQuest* quest = (*active_quest_iterator); // Doing this for readability sake
				if (!is_quest_active(quest) || is_quest_complete(quest)) // Has the active flag been removed or the completed flag been set?
				{
					// Current quest is no longer active
					// Was the quest completed or failed?
					std::string quest_name = quest->GetFullName() ? quest->GetFullName()->name.CStr() : "<no name>";
					std::string flagString = "";
					std::vector<bool> quest_flags = get_quest_flag_states(quest);
					for (auto flag : quest_flags)
					{
						flagString += flag ? "1" : "0";
					}

					if (is_quest_complete(quest) || is_quest_failed(quest)) // Has the quest been (completed or failed) or was the active flag just removed for some reason?
					{
						if (!is_quest_failed(quest))
						{
							_MESSAGE("Quest '%s' '%s' Completed! %s", int_to_hex_string(quest->refID).c_str(), quest_name.c_str(), flagString.c_str());
							// TODO Tell server
							json quest_info =
							{
								{"ID", int_to_hex_string(quest->refID)},
								{"Stage", std::to_string(quest->currentStage)},
								{"Name", quest_name},
								{"Flags", flagString}
							};
							QSyncMessage msg(message_type::QUEST_COMPLETED, quest_info.dump());
							client.send_message(msg.toString());
						}
						else
						{
							_MESSAGE("Quest '%s' '%s' Failed! %s", int_to_hex_string(quest->refID).c_str(), quest_name.c_str(), flagString.c_str());
							// TODO Tell Server
							json quest_info =
							{
								{"ID", int_to_hex_string(quest->refID)},
								{"Stage", std::to_string(quest->currentStage)},
								{"Name", quest_name},
								{"Flags", flagString}
							};
							QSyncMessage msg(message_type::QUEST_FAILED, quest_info.dump());
							client.send_message(msg.toString());
						}
					}
					else
					{
						_MESSAGE("Quest '%s' '%s' Has been removed from the active list and is not completed or failed! %s", int_to_hex_string(quest->refID).c_str(), quest_name.c_str(), flagString.c_str());
						// TODO Tell server? Does it really need to know about this? idk
						json quest_info =
						{
							{"ID", int_to_hex_string(quest->refID)},
							{"Stage", std::to_string(quest->currentStage)},
							{"Name", quest_name},
							{"Flags", flagString}
						};
						QSyncMessage msg(message_type::QUEST_INACTIVE, quest_info.dump());
						client.send_message(msg.toString());
					}
					// Remove this quest from the list
					g_current_quest_list.erase(active_quest_iterator++); // First increments the iterator with ++, then removes the previous node as using ++ returns the node you are incrementing from
				}
				else
				{
					++active_quest_iterator; // Move to the next node, this one is still active
				}
			}
		}

		//g_consoleInterface->RunScriptLine("GetQuestCompleted 10a214", nullptr);
		
	}
		break;
	case NVSEMessagingInterface::kMessage_ScriptCompile: break;
	case NVSEMessagingInterface::kMessage_EventListDestroyed: break;
	case NVSEMessagingInterface::kMessage_PostQueryPlugins: 
		Console_Print("Post Query Plugins - plugintest running");
		break;
	default: break;
	}
}

std::string int_to_hex_string(int number)
{
	std::stringstream stream;
	stream << std::hex << number;
	return std::string(stream.str());
}

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
	_MESSAGE("query");

	// fill out the info structure
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "QuestSyncPlugin";
	info->version = 3;

	// version checks
	//if (nvse->nvseVersion < PACKED_NVSE_VERSION)
	if(false)
	{
		_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, PACKED_NVSE_VERSION);
		return false;
	}

	if (!nvse->isEditor)
	{
		if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_0_525)
		{
			_ERROR("incorrect runtime version (got %08X need at least %08X)", nvse->runtimeVersion, RUNTIME_VERSION_1_4_0_525);
			return false;
		}

		if (nvse->isNogore)
		{
			_ERROR("NoGore is not supported");
			return false;
		}
	}
	else
	{
		if (nvse->editorVersion < CS_VERSION_1_4_0_518)
		{
			_ERROR("incorrect editor version (got %08X need at least %08X)", nvse->editorVersion, CS_VERSION_1_4_0_518);
			return false;
		}
	}
	
	// version checks pass
	// any version compatibility checks should be done here
	return true;
}

bool NVSEPlugin_Load(NVSEInterface* nvse)
{
	_MESSAGE("load");

	g_pluginHandle = nvse->GetPluginHandle();
	//nvse->GetRuntimeDirectory();

	// save the NVSE interface in case we need it later
	g_nvseInterface = nvse;

	// register to receive messages from NVSE
	g_messagingInterface = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
	g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", MessageHandler);

	if (!nvse->isEditor)
	{
#if RUNTIME
		// script and function-related interfaces
		g_script = static_cast<NVSEScriptInterface*>(nvse->QueryInterface(kInterface_Script));
		g_stringInterface = static_cast<NVSEStringVarInterface*>(nvse->QueryInterface(kInterface_StringVar));
		g_arrayInterface = static_cast<NVSEArrayVarInterface*>(nvse->QueryInterface(kInterface_ArrayVar));
		g_dataInterface = static_cast<NVSEDataInterface*>(nvse->QueryInterface(kInterface_Data));
		g_eventInterface = static_cast<NVSEEventManagerInterface*>(nvse->QueryInterface(kInterface_EventManager));
		g_serializationInterface = static_cast<NVSESerializationInterface*>(nvse->QueryInterface(kInterface_Serialization));
		g_consoleInterface = static_cast<NVSEConsoleInterface*>(nvse->QueryInterface(kInterface_Console));
		ExtractArgsEx = g_script->ExtractArgsEx;
#endif
	}
	

	/***************************************************************************
	 *
	 *	READ THIS!
	 *
	 *	Before releasing your plugin, you need to request an opcode range from
	 *	the NVSE team and set it in your first SetOpcodeBase call. If you do not
	 *	do this, your plugin will create major compatibility issues with other
	 *	plugins, and will not load in release versions of NVSE. See
	 *	nvse_readme.txt for more information.
	 *
	 *	See https://geckwiki.com/index.php?title=NVSE_Opcode_Base
	 *
	 **************************************************************************/

	// Do NOT use this value when releasing your plugin; request your own opcode range.
	UInt32 const examplePluginOpcodeBase = 0x2000;
	
	 // register commands
	nvse->SetOpcodeBase(examplePluginOpcodeBase);
	
	/*************************
	 * The hexadecimal Opcodes are written as comments to the left of their respective functions.
	 * It's important to keep track of how many Opcodes are being used up,
	 * as each plugin is given a limited range which may need to be expanded at some point.
	 *
	 * === How Opcodes Work ===
	 * Each function is associated to an Opcode,
	 * which the game uses to look-up where to find your function's code.
	 * It is CRUCIAL to never change a function's Opcode once it is released to the public.
	 * This is because when compiling a script, each function being called are represented as Opcodes.
	 * So changing a function's Opcode will invalidate previously compiled scripts,
	 * as they will fail to look up that function properly, instead probably finding some other function.
	 *
	 * Example: say we compile a script that uses ExamplePlugin_IsNPCFemale.
	 * The compiled script will check for the Opcode 0x2002 to call that function, and should work fine.
	 * If we remove /REG_CMD(ExamplePlugin_CrashScript);/, and don't register a new function to replace it,
	 * then `REG_CMD(ExamplePlugin_IsNPCFemale);` now registers with Opcode #0x2001.
	 * When we test the script now, a bug/crash is bound to happen,
	 * since the script is looking for an Opcode which is no longer bound to the expected function.
	 ************************/
	
	/*2000* RegisterScriptCommand(ExamplePlugin_PluginTest);
	/*2001* REG_CMD(ExamplePlugin_CrashScript);
	/*2002* REG_CMD(ExamplePlugin_IsNPCFemale);
	/*2003* REG_CMD(ExamplePlugin_FunctionWithAnAlias);
	/*2004* REG_TYPED_CMD(ExamplePlugin_ReturnForm, Form);
	/*2005* REG_TYPED_CMD(ExamplePlugin_ReturnString, String);	// ignore the highlighting for String class, that's not being used here.
	/*2006* REG_TYPED_CMD(ExamplePlugin_ReturnArray, Array);*/
	
	return true;
}
