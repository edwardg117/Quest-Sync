// Build for xNVSE 6.2.4
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

#ifndef RegisterScriptCommand
#define RegisterScriptCommand(name) 	nvse->RegisterCommand(&kCommandInfo_ ##name);
#endif

IDebugLog		gLog("Quest_Sync.log");
PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

NVSEMessagingInterface* g_messagingInterface;
NVSEInterface* g_nvseInterface;
NVSECommandTableInterface* g_cmdTable;
const CommandInfo* g_TFC;

#if RUNTIME  //if non-GECK version (in-game)
NVSEScriptInterface* g_script;
NVSEConsoleInterface* g_consoleInterface{};
TCPClient client("", 0);
#endif

bool (*ExtractArgsEx)(COMMAND_ARGS_EX, ...);

// Forward declarations of my functions
UInt32 g_previousQuestCount = 0;
const long long g_wait_for_reconnect_seconds = 60;
std::chrono::steady_clock::time_point g_last_connection_failure = std::chrono::steady_clock::now();

std::list<TESQuest*> g_current_quest_list;
std::list<BGSQuestObjective*> g_current_objective_list;
QuestManager g_QuestManager;

// This is a message handler for nvse events
// With this, plugins can listen to messages such as whenever the game loads
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
	//TESQuest* test = new TESQuest();
	//test->lVarOrObjectives.Count();
	// This is purely to test code signing.
	switch (msg->type)
	{
	case NVSEMessagingInterface::kMessage_LoadGame:
		_MESSAGE("Received load game message with file path %s", msg->data);
		break;
	case NVSEMessagingInterface::kMessage_SaveGame:
		_MESSAGE("Received save game message with file path %s", msg->data);
		break;
	case NVSEMessagingInterface::kMessage_PreLoadGame:
		_MESSAGE("Received pre load game message with file path %s", msg->data);
		break;
	case NVSEMessagingInterface::kMessage_PostLoadGame:
		_MESSAGE("Received post load game message", msg->data ? "Error/Unkwown" : "OK");
		_MESSAGE("Game has been loaded (Save loaded), populating current quests");
		//populate_current_quests();
		g_QuestManager.populate_current_quests(PlayerCharacter::GetSingleton()->questObjectiveList);
		_MESSAGE("Done!");
		break;
	case NVSEMessagingInterface::kMessage_PostLoad:
		_MESSAGE("Received post load plugins message");
		break;
	case NVSEMessagingInterface::kMessage_PostPostLoad:
		_MESSAGE("Received post post load plugins message");
		break;
	case NVSEMessagingInterface::kMessage_ExitGame:
		_MESSAGE("Exiting Game");
		g_QuestManager.reset();
		client.Disconnect();
		client.Cleanup();
		break;
	case NVSEMessagingInterface::kMessage_ExitGame_Console:
		_MESSAGE("Exiting Game - via console qqq comand");
		g_QuestManager.reset();
		client.Disconnect();
		client.Cleanup();
		break;
	case NVSEMessagingInterface::kMessage_ExitToMainMenu:
		_MESSAGE("Exiting to Main Menu");
		g_QuestManager.reset();
		break;
	case NVSEMessagingInterface::kMessage_Precompile:
		_MESSAGE("Received precompile message with script");
		break;
	case NVSEMessagingInterface::kMessage_RuntimeScriptError:
		_MESSAGE("Received runtime script error message %s", msg->data);
		break;
	case NVSEMessagingInterface::kMessage_NewGame: // This wasn't here but appears to be defined
		_MESSAGE("New Game - plugintest running");
		//populate_current_quests();
		g_QuestManager.populate_current_quests(PlayerCharacter::GetSingleton()->questObjectiveList);
		break;
	case NVSEMessagingInterface::kMessage_DeferredInit: // Same as kMessage_NewGame: not present but seems to be defined
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
	case NVSEMessagingInterface::kMessage_MainGameLoop: // Same as above
		if (!client.isConnected()) // Can't do anything if the server isn't connected
		{
			// uint64_t sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
			//auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

			if (std::chrono::duration_cast<std::chrono::seconds>(now - g_last_connection_failure).count() >= g_wait_for_reconnect_seconds)
			{
				// Try connect
				//_MESSAGE("%llu is greater than %llu", std::chrono::duration_cast<std::chrono::seconds>(now - g_last_connection_failure).count(), g_wait_for_reconnect_seconds);
				client.Cleanup(); // cleanup any garbage
				client.init();
				client.Connect();					// 4 294 967 295
				//g_last_connection_failure = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				g_last_connection_failure = std::chrono::steady_clock::now();

				if (!client.isConnected()) { _MESSAGE("Unable to connect to the Quest Sync server, will try again in %llu seconds :(", g_wait_for_reconnect_seconds); Console_Print("Unable to connect to the Quest Sync server, will try again in %llu seconds :(", g_wait_for_reconnect_seconds); break; } // Can't do anything if not connected
				else
				{
					_MESSAGE("Connected to the server!");
					Console_Print("Connected to the Quest Sync server!");
					g_QuestManager.populate_current_quests(PlayerCharacter::GetSingleton()->questObjectiveList);
					std::string message = "MessageEx \"Connected to Quest Sync Serevr!\"";
					g_consoleInterface->RunScriptLine(message.c_str(), nullptr);
				}
			}
		}
		if (!g_QuestManager.isSyncedWithServer() && g_QuestManager.completedIntro())
		{
			if (g_QuestManager.getSyncState() == 0) { g_QuestManager.syncWithServer(&client); }

		}
		//else
		{
			PlayerCharacter* player = PlayerCharacter::GetSingleton();
			g_QuestManager.process(&client, player->questObjectiveList, g_consoleInterface);
			//g_last_connection_failure = std::chrono::steady_clock::now();
		}
	default:
		break;
	}
}


bool Cmd_ExamplePlugin_PluginTest_Execute(COMMAND_ARGS);

#if RUNTIME  //if non-GECK version (in-game)
//In here we define a script function
//Script functions must always follow the Cmd_FunctionName_Execute naming convention
bool Cmd_ExamplePlugin_PluginTest_Execute(COMMAND_ARGS)
{
	_MESSAGE("plugintest");

	*result = 42;

	Console_Print("plugintest running");

	return true;
}
#endif

//This defines a function without a condition, that does not take any arguments
DEFINE_COMMAND_PLUGIN(ExamplePlugin_PluginTest, "prints a string", false, NULL)

bool Cmd_ExamplePlugin_IsNPCFemale_Eval(COMMAND_ARGS_EVAL);

#if RUNTIME
//Conditions must follow the Cmd_FunctionName_Eval naming convention
bool Cmd_ExamplePlugin_IsNPCFemale_Eval(COMMAND_ARGS_EVAL)
{

	TESNPC* npc = (TESNPC*)arg1;
	*result = npc->baseData.IsFemale() ? 1 : 0;
	return true;
}
#endif

bool Cmd_ExamplePlugin_IsNPCFemale_Execute(COMMAND_ARGS);

#if RUNTIME
bool Cmd_ExamplePlugin_IsNPCFemale_Execute(COMMAND_ARGS)
{
	//Created a simple condition 
	//thisObj is what the script extracts as parent caller
	//EG, Ref.IsFemale would make thisObj = ref
	//We are using actor bases though, so the function is called as such: ExamplePlugin_IsNPCFemale baseForm
	TESNPC* npc = 0;
	if (ExtractArgsEx(EXTRACT_ARGS_EX, &npc))
	{
		Cmd_ExamplePlugin_IsNPCFemale_Eval(thisObj, npc, NULL, result);
	}

	return true;
}
#endif
DEFINE_COMMAND_PLUGIN(ExamplePlugin_IsNPCFemale, "Checks if npc is female", false, kParams_OneActorBase)

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
	_MESSAGE("query");

	// fill out the info structure
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "QuestSyncPlugin";
	info->version = 9;

	// version checks
	if (nvse->nvseVersion < PACKED_NVSE_VERSION)
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

	// Should not use any later than 6.2.4 for this version
	if (nvse->nvseVersion < PACKED_NVSE_VERSION)
	{
		_ERROR("NVSE version too new (got %08X expected %08X)", nvse->nvseVersion, PACKED_NVSE_VERSION);
		return false;
	}

	// version checks pass
	// any version compatibility checks should be done here
	return true;
}

bool NVSEPlugin_Load(const NVSEInterface* nvse)
{
	_MESSAGE("load");

	g_pluginHandle = nvse->GetPluginHandle();

	// save the NVSEinterface in cas we need it later
	g_nvseInterface = (NVSEInterface*)nvse;

	// register to receive messages from NVSE
	g_messagingInterface = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
	g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", MessageHandler);

#if RUNTIME
	g_script = (NVSEScriptInterface*)nvse->QueryInterface(kInterface_Script);
	ExtractArgsEx = g_script->ExtractArgsEx;
#endif
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

	 // register commands
	nvse->SetOpcodeBase(0x2000);
	//RegisterScriptCommand(ExamplePlugin_PluginTest);
	//RegisterScriptCommand(ExamplePlugin_IsNPCFemale);

	return true;
}
