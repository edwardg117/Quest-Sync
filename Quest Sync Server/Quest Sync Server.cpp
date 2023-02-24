// Quest Sync Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "TCPListener.h"

void Listener_MessageReceived(TCPListener* listener, int client, std::string msg);
static inline void ltrim(std::string& s);
static inline void rtrim(std::string& s);
static inline void trim(std::string& s);
std::map<std::string, std::string> GetConfig();

std::vector<std::string> g_questBlacklist = {
        "104c1c",   // Ain't That a Kick in the Head
        "10a214"          // Back in the Saddle
};

struct quest_and_objectives
{
    std::string ID;
    std::string Name;
    std::list<std::string> ActiveStages;
};

std::list<quest_and_objectives> g_active_quest_list;

int main(int argc, char* argv[])
{
    // Setup and config read
    std::string ipAddress = "127.0.0.1"; // Example values used here, they will be overwritten
    int port = 25575;
    std::cout << "Getting things ready..." << std::endl;

    if (argc == 3)
    {
        ipAddress = argv[1];
        port = atoi(argv[2]);
    }
    else
    {
        std::map<std::string, std::string> config = GetConfig();
        ipAddress = config["IpAddress"];
        port = stoi(config["Port"]);
    }

    std::cout << "Config ready... " << std::endl;
    //TCPListener server(ipAddress, port, Listener_MessageReceived); // Create server object
    TCPListener server(ipAddress, port, Listener_MessageReceived); // Create server object
    if (server.init())
    {
        std::cout << "Starting Quest Sync Server on " << ipAddress << ":" << port << std::endl;
        server.Run();
    }
    else
    {
        std::cerr << "Unable to init Winsock! Must exit!";
    }

    return 0;
}

bool is_new_quest(std::string refID)
{
    bool result = true;
    for (auto quest : g_active_quest_list)
    {
        if (quest.ID == refID) { result = false; break; } // If refID is the same, it's this quest!
    }
    return result;
}
// This function runs whenever a client sends a message to the server
void Listener_MessageReceived(TCPListener* listener, int client, std::string msg)
{ // Do server logic here
    //std::cout << "I got a message!";
    // 
    //listener->Send_to_specific(client, msg);

    //std::vector<int> excludeVector = { client };
    //listener->Send_to_all(msg, &excludeVector);

    //listener->Send_to_all(msg);
    //auto parsed_message = json::parse(msg);
    //message_type m_type;
    //parsed_message["message_type"].get_to(m_type);
    QSyncMessage message (msg);

    switch (message.type)
    {
    case message_type::NEW_QUEST: 
    {
        auto quest_info = json::parse(message.body);
        //std::cout << message.body << std::endl;
        quest_and_objectives new_quest;
        quest_info["ID"].get_to(new_quest.ID);
        std::cout << client << ": New Quest - " << quest_info["ID"] << " " << quest_info["Name"] << " " << quest_info["Stage"] << " " << quest_info["Flags"] << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest_info["ID"]) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        if (!is_new_quest(new_quest.ID))
        {
            std::cout << "Quest has already been received, ignoring" << std::endl;
            break;
        }

        //quest_and_objectives new_quest;
        //quest_info["ID"].get_to(new_quest.ID);
        quest_info["Name"].get_to(new_quest.Name);
        std::string stage;
        quest_info["Stage"].get_to(stage);
        new_quest.ActiveStages.push_back(stage);

        g_active_quest_list.push_back(new_quest);

        message.type = message_type::START_QUEST;
        std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
        listener->Send_to_all(message.toString(), &excluded);
    }
    break;
    case message_type::QUEST_UPDATED: 
    {
        auto quest_info = json::parse(message.body);
        std::cout << client << ": Quest Updated - " << quest_info["ID"] << " " << quest_info["Name"] << " " << quest_info["Stage"] << " " << quest_info["Flags"] << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest_info["ID"]) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        message.type = message_type::UPDATE_QUEST;
        std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
        listener->Send_to_all(message.toString(), &excluded);
    }
    break;
    case message_type::QUEST_COMPLETED: 
    {
        auto quest_info = json::parse(message.body);
        std::cout << client << ": Quest Completed - " << quest_info["ID"] << " " << quest_info["Name"] << " " << quest_info["Stage"] << " " << quest_info["Flags"] << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest_info["ID"]) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        message.type = message_type::COMPLETE_QUEST;
        std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
        listener->Send_to_all(message.toString(), &excluded);
    }
    break;
    case message_type::QUEST_FAILED: 
    {
        auto quest_info = json::parse(message.body);
        std::cout << client << ": Quest Failed - " << quest_info["ID"] << " " << quest_info["Name"] << " " << quest_info["Stage"] << " " << quest_info["Flags"] << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest_info["ID"]) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        message.type = message_type::FAIL_QUEST;
        std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
        listener->Send_to_all(message.toString(), &excluded);
    }
        break;
    case message_type::REQUEST_ALL_QUEST_STATES: break;
    case message_type::REQUEST_CURRENT_QUESTS_COMPLETION: 
    {
        json current_quests_info = json::array();
        for (auto quest : g_active_quest_list)
        {
            json quest_info =
            {
                {"ID", quest.ID},
                {"Stages", quest.ActiveStages},
                {"Name", quest.Name}
            };
            current_quests_info.push_back(quest_info.dump());
        }

        QSyncMessage message(CURRENT_QUESTS_COMPLETEION, current_quests_info.dump());

        listener->Send_to_specific(client, message.toString());

    }
        break;
    case message_type::RESEND_CONN_ACK:
    {
        int itime;
        //std::string stime;
        itime = stoi(message.body);
        //itime = stoi(stime);
        Sleep(itime * (1000));
        json json_message = {
            {"message_type", message_type::RESEND_CONN_ACK},
            {"message_contents", ""}
        };
        listener->Send_to_specific(client, json_message.dump());
    }
        break;
    
    case message_type::QUEST_INACTIVE: 
    {
        auto quest_info = json::parse(message.body);
        std::cout << client << ": Quest Inactive - " << quest_info["ID"] << " " << quest_info["Name"] << " " << quest_info["Stage"] << " " << quest_info["Flags"] << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest_info["ID"]) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        message.type = message_type::INACTIVE_QUEST;
        std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
        listener->Send_to_all(message.toString(), &excluded);
    }
        break;
    case message_type::OBJECTIVE_COMPLETED:
    {
        auto stage_info = json::parse(message.body);
        std::cout << client << ": Objective " << stage_info["Stage"] << " completed for " << stage_info["ID"] << std::endl;
    }
        break;
    default: break;
    }
}

/**
* Code for trimming leading and trailing whitespace taken from https://stackoverflow.com/questions/216823/how-to-trim-an-stdstring
*/
// trim from start (in place)
static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
    rtrim(s);
    ltrim(s);
}

std::map<std::string, std::string> GetConfig()
{
    std::string filename = "Quest Sync Server.cfg";
    std::map<std::string, std::string> details;
    std::string record;
    std::string key;
    std::string value;
    std::ifstream configFile(filename);
    if (configFile.is_open())
    {
        //Open
        while (!configFile.eof())
        {
            std::getline(configFile, record); // Read record
            // trim whitespace
            trim(record);
            // To avoid nesting hard, I'm using continue to skip lines
            if (record[0] == ';') { continue; } // If line is a comment
            if (record[0] == '[') { continue; } // Line is probably a section header
            // Getting connection details
            //std::cout << "[Connection Details]";
            //std::cout << record << std::endl;
            key = record.substr(0, record.find("="));
            trim(key);
            value = record.substr(record.find("=") + 1, record.length() - 1);
            trim(value);
            //std::cout << "'" << key << ":" << value << "'" << std::endl;
            details[key] = value;
        }
    }
    else
    {
        // Write default I guess
        std::ofstream configFile(filename, std::ios::app);
        if (configFile.is_open())
        {
            configFile << "[Server]\n";
            configFile << "IpAddress=127.0.0.1\n";
            configFile << "Port=25575";
            configFile.close();
        }
        else
        {
            std::cerr << "Unable to read/write config file! Check permissions and try again!\nProceeding with default values of 127.0.0.1 and 25575";
        }
        details["IpAddress"] = "127.0.0.1";
        details["Port"] = "25575";
    }

    return details;
}