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
#include "QuestManager.h"
#include "nvse/GameForms.h"
#include "Quest Sync Server/QsyncDefinitions.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <chrono>

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


// Forward declarations of my functions
std::string int_to_hex_string(int number);
bool is_new_quest(std::string ID);
bool is_new_quest(UInt32 refID);
bool is_new_objective(std::string QuestID, std::string objectiveId);
bool is_new_objective(UInt32 refID, UInt32 objectiveId);
bool is_new_objective(BGSQuestObjective* Objective);
UInt32 g_previousQuestCount = 0;
const long long g_wait_for_reconnect_seconds = 60;
std::chrono::steady_clock::time_point g_last_connection_failure = std::chrono::steady_clock::now();

struct q_qsync_states g_qsync_states;
std::list<TESQuest*> g_current_quest_list;
std::list<BGSQuestObjective*> g_current_objective_list;
QuestManager g_QuestManager;

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
// Returns if the objective is completed
bool is_objective_completed(BGSQuestObjective* objective)
{
	return (objective->status & 2) == 2;
}
// Populates the current quest and objective lists. Removes all data stored in the lists before starting (essentially reseting it)
// TODO, remove in favour of QuestManager method
[[DEPRECATED]]
void populate_current_quests()
{
	PlayerCharacter* player = PlayerCharacter::GetSingleton();
	auto iterator = player->questObjectiveList.Head(); // This is what I'll use to iterate through the list (sort of)
	_MESSAGE("Re-populating the current quest and objective lists...");
	g_current_quest_list.clear(); // Just in case this is called while there's already stuff in it, this function shouldn't cause problems by resetting the positions of these things in any case
	g_current_objective_list.clear();
	std::list<TESQuest*> ignored_quests;

	for (int current_new_quest = 0; current_new_quest < player->questObjectiveList.Count(); current_new_quest++) // Do ALL quest entries in the pip-boy
	{
		TESQuest* new_quest = iterator->data->quest; // The quest attached to the current entry
		UInt32 stage = iterator->data->objectiveId; // The quest stage as it appears on the fallout wiki
		// Is this a new quest or an entry for an existing one?
		bool ignored = false;
		for (auto ignored_quest : ignored_quests) // Just to make logs look a bit nicer
		{
			if (ignored_quest->refID == new_quest->refID) { ignored = true; break; } // If reference ids match, it's the same! (duh)
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

		if (is_new_quest(new_quest->refID) && is_quest_active(new_quest) && !is_quest_complete(new_quest)) // Quest can remain active when complete
		{
			// Quest is active and hasn't been added before, add it now
			g_current_quest_list.push_back(new_quest); // Add to the list fo quests I'm monitoring for completeion
			_MESSAGE("Added Quest '%s' '%s' Flags: %s", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), flagString.c_str());
		}
		else if(is_new_quest(new_quest->refID) && ignored)
		{
			_MESSAGE("Quest '%s' '%s' Flags: %s not added.", int_to_hex_string(new_quest->refID).c_str(), quest_name.c_str(), flagString.c_str());
			ignored_quests.push_back(new_quest);
		}

		if (is_new_objective(iterator->data) && !is_objective_completed(iterator->data) && !is_quest_complete(new_quest) && !is_quest_failed(new_quest)) // If a quest is failed, the objective is not marked as completed. Check for that too. It might be possible for a quest to be inactive but receive a new objective before it activates
		{
			g_current_objective_list.push_back(iterator->data);
			_MESSAGE("Stage is not marked as complete, adding to the current objective list.");
		}
		iterator = iterator->next;
	}



	g_previousQuestCount += player->questObjectiveList.Count(); // Update the number of quest objectives that were in the list on this run so I can compare to it next run
}
// Resets the state of all Quest Sync related variables to what they should be in the main menu


//TCPClient client("", 0);
// This is a message handler for nvse events
// With this, plugins can listen to messages such as whenever the game loads
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case NVSEMessagingInterface::kMessage_PostLoad:
		//_MESSAGE("Post Load - plugintest running");
		//TCPClient client("127.0.0.1", 25575);

		break;
	case NVSEMessagingInterface::kMessage_ExitGame:
		_MESSAGE("Exit Game - plugintest running");
		_MESSAGE("Exiting Game");
		g_QuestManager.reset();
		client.Disconnect();
		client.Cleanup();
		break;
	case NVSEMessagingInterface::kMessage_ExitToMainMenu:
		_MESSAGE("Exiting to Main Menu");
		g_QuestManager.reset();
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
		_MESSAGE("Exiting Game");
		g_QuestManager.reset();
		client.Disconnect();
		client.Cleanup();
		break;
	case NVSEMessagingInterface::kMessage_PostLoadGame:
		_MESSAGE("Game has been loaded (Save loaded)");
		populate_current_quests();
		_MESSAGE("Done!");
		break;
	case NVSEMessagingInterface::kMessage_PostPostLoad:
		//_MESSAGE("Post Post Load - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_RuntimeScriptError: break;
	case NVSEMessagingInterface::kMessage_DeleteGame: break;
	case NVSEMessagingInterface::kMessage_RenameGame: break;
	case NVSEMessagingInterface::kMessage_RenameNewGame: break;
	case NVSEMessagingInterface::kMessage_NewGame:
		_MESSAGE("New Game - plugintest running");
		populate_current_quests();
		break;
	case NVSEMessagingInterface::kMessage_DeleteGameName:
		_MESSAGE("Delete Game Name - plugintest running");
		break;
	case NVSEMessagingInterface::kMessage_RenameGameName: break;
	case NVSEMessagingInterface::kMessage_RenameNewGameName: break;
	case NVSEMessagingInterface::kMessage_DeferredInit:
		_MESSAGE("Initialising TCP Client");
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
			Console_Print("Connected to the Quest Sync server!");
		}
		else
		{
			_MESSAGE("Unable to connect :(");
			Console_Print("Unable to connect to the Quest Sync server, will try again in %llu seconds :(", g_wait_for_reconnect_seconds);
			std::cout << WSAGetLastError() << std::endl;
		}
		break;
	case NVSEMessagingInterface::kMessage_ClearScriptDataCache: break;
	case NVSEMessagingInterface::kMessage_MainGameLoop:
		// Overview of flow: (outdated, moved server message processing to last)
		// 
		// Connected?
		// No - Try connect again
		// 
		// 1. Get messages from server and apply actions
		//	1.1 Connection Acknowledgement?
		//		1.1.1 Set Connection Acknowledgement to true
		//	1.2 Start Quest?
		//		1.2.1 Has this quest already been started?
		//		1.2.2 (Yes) Do nothing
		//		1.2.3 (No) Start the quest and set its stage
		//	1.3 Update Quest?
		//		1.3.1 Set Stage
		//		1.3.2 Complete previous stage with SetObjectiveCompleted
		//	1.4 Complete Quest?
		//		1.4.1 Set Stage? Complete previous stage?
		//		1.4.2 Complete quest
		//	1.5 Fail Quest?
		//		1.5.1 Fail Quest
		//	1.6 All Quests - Sync Quests
		//		1.6.1 Figure out what quests differ
		//		1.6.2 Start missing quests
		//		1.6.3 Set Relevant Stages
		//		1.6.4 Complete quests
		//		1.6.5 Fail Quests
		//		1.6.6 Send the server any discrepancies (In case this client started a quest the server doesn't know about)
		// 2. Look for Quest and Stage updates 
		//	2.1 Has the list of quest objectives increased in count?
		//		2.1.1 Get latest addition/s by looping from the head of the list, and moving down for the number of new entries
		//		      as all the latest additions are insterted at the top of the list.
		//		2.1.2 New (ACTIVE) quest that was just added? 
		//			2.1.2.1 (Yes) Add to the currently monitored quest list
		//			2.1.2.2 Tell the server about this new quest (with stage)
		//		2.1.3 Existing (ACTIVE) quest that has a new entry?
		//			2.1.3.1 (Yes) Tell the server about this new entry (stage)
		//		If neither 2.1.2 or 2.1.3 then ignore this quest, but make a note of it in the logs because there might be something I have to do with them, idk  yet
		// 
		// 3. Check for quest completions/failures by - For every currently monitored quest:
		//	3.1 Check if Active or Completed
		//	(YES)
		//		3.1.1 If Completed or Failed
		//		(YES) 
		//			3.1.1.1 If Completed - Tell Server
		//			3.1.1.2 If Failed - Tell Server
		//		(NO)
		//			3.1.1.3 Else Quest was removed from active for some reason, will most likely get added again later
		//		3.1.2 Remove Quest from Active list
		//	(NO)
		//	3.2 Move on, quest will still be monitored 
		//		

		if (!client.isConnected()) // Can't do anything if the server isn't connected
		{
			// uint64_t sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
			//auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

			if(std::chrono::duration_cast<std::chrono::seconds>(now - g_last_connection_failure).count() >= g_wait_for_reconnect_seconds)
			{
				// Try connect
				//_MESSAGE("%llu is greater than %llu", std::chrono::duration_cast<std::chrono::seconds>(now - g_last_connection_failure).count(), g_wait_for_reconnect_seconds);
				client.Cleanup(); // cleanup any garbage
				client.init();
				client.Connect();					// 4 294 967 295
				//g_last_connection_failure = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				g_last_connection_failure = std::chrono::steady_clock::now();

				if (!client.isConnected()) { _MESSAGE("Unable to connect to the Quest Sync server, will try again in %llu seconds :(", g_wait_for_reconnect_seconds); Console_Print("Unable to connect to the Quest Sync server, will try again in %llu seconds :(", g_wait_for_reconnect_seconds); break; } // Can't do anything if not connected
				else { _MESSAGE("Connected to the server!"); Console_Print("Connected to the Quest Sync server!"); }
			}
		}
		if (!g_QuestManager.isSyncedWithServer() && g_QuestManager.completedIntro())
		{
			if (g_QuestManager.getSyncState() == 0) { g_QuestManager.syncWithServer(&client); }

		}
		{
			PlayerCharacter* player = PlayerCharacter::GetSingleton();
			g_QuestManager.process(&client, player->questObjectiveList, g_consoleInterface);
		}

		break;
	case NVSEMessagingInterface::kMessage_ScriptCompile: break;
	case NVSEMessagingInterface::kMessage_EventListDestroyed: break;
	case NVSEMessagingInterface::kMessage_PostQueryPlugins: break;
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
	info->version = 7;

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

bool is_new_quest(std::string ID) // Takes hex id for quest and checks to see if it's new
{
	bool is_new_quest = true;
	for (auto existing_quest : g_current_quest_list)
	{
		if (int_to_hex_string(existing_quest->refID) == ID) { is_new_quest = false; break; }
	}
	return is_new_quest;
}
bool is_new_quest(UInt32 refID)
{
	bool is_new = true;
	for (auto existing_quest : g_current_quest_list)
	{
		if (existing_quest->refID == refID) { is_new = false; break; } // If reference ids match, it's the same! (duh)
	}
	return is_new;
}

bool is_new_objective(std::string QuestID, std::string objectiveId)
{
	bool is_new_objective = true;
	for (auto existing_objective : g_current_objective_list)
	{
		if (existing_objective->objectiveId == stoi(objectiveId) && int_to_hex_string(existing_objective->quest->refID) == QuestID) { is_new_objective = false; break; }
	}
	return is_new_objective;
}
bool is_new_objective(UInt32 refID, UInt32 objectiveId)
{
	bool is_new_objective = true;
	for (auto existing_objective : g_current_objective_list)
	{
		if (existing_objective->objectiveId == objectiveId && existing_objective->quest->refID == refID) { is_new_objective = false; break; } // If objective ids match, it's the same!
	}
	return is_new_objective;
}
bool is_new_objective(BGSQuestObjective* Objective)
{
	bool is_new_objective = true;
	UInt32 objectiveId = Objective->objectiveId;
	UInt32 refID = Objective->quest->refID;
	for (auto existing_objective : g_current_objective_list)
	{
		if (existing_objective->objectiveId == objectiveId && existing_objective->quest->refID == refID) { is_new_objective = false; break; } // If objective ids match, it's the same!
	}
	return is_new_objective;
}
