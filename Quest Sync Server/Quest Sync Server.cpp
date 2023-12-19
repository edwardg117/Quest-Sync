// Quest Sync Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "TCPListener.h"
#include "Version.h"

void Listener_MessageReceived(TCPListener* listener, int client, std::string msg);
static inline void ltrim(std::string& s);
static inline void rtrim(std::string& s);
static inline void trim(std::string& s);
std::map<std::string, std::string> GetConfig();
enum class serverAction;
serverAction process_Message(int client, json message, QSyncMsgBody* replyAllBody, QSyncMsgBody* replyClientBody);

std::vector<std::string> g_questBlacklist = {
        "104c1c",   // Ain't That a Kick in the Head
        "10a214"          // Back in the Saddle
};

//struct quest_and_objectives
//{
//    std::string ID;
//    std::string Name;
//    bool Active = true;
//    bool Completed = false;
//    bool Failed = false;
//    std::vector<stage> Stages;
//
//    bool operator<(const quest_and_objectives& other) const
//    {
//        return ID < other.ID;
//    }
//};
enum class serverAction {
    NONE,
    SHUTDOWN
};
std::vector<QSyncQuest> g_quest_list;

int main(int argc, char* argv[])
{
    std::cout << "Quest Sync Server v" << Version::ServerVersion[0] << "." << Version::ServerVersion[1] << std::endl;
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
// An iterative binary search function.
int binarySearch(std::vector<QSyncQuest> arr, int l, int r, std::string refID)
{
    while (l <= r) {
        int m = l + (r - l) / 2;

        // Check if x is present at mid
        if (arr[m].GetRefId() == refID)
            return m;

        // If x greater, ignore left half
        if (arr[m].GetRefId() < refID)
            l = m + 1;

        // If x is smaller, ignore right half
        else
            r = m - 1;
    }

    // If we reach here, then element was not present
    return -1;
}
//bool is_new_quest(std::string refID) // TODO binary search
//{
//    //bool result = true;
//    //for (auto quest : g_quest_list)
//    //{
//    //    if (quest.GetRefId() == refID) { result = false; break; } // If refID is the same, it's this quest!
//    //}
//    //return result;
//
//   return std::binary_search(g_quest_list.begin(), g_quest_list.end(), refID);
//}
bool is_new_quest(QSyncQuest quest) // TODO binary search
{
    //bool result = true;
    //for (auto quest : g_quest_list)
    //{
    //    if (quest.GetRefId() == refID) { result = false; break; } // If refID is the same, it's this quest!
    //}
    //return result;
    //bool yes = std::binary_search(g_quest_list.begin(), g_quest_list.end(), quest);

    return !std::binary_search(g_quest_list.begin(), g_quest_list.end(), quest);
}

bool is_stage_updated(std::string refID, std::string stageID) // TODO binary search
{
    /*bool result = true;
    for (auto quest : g_quest_list)
    {
        if(quest.GetRefId() == refID)
        {
            for (auto stage : quest.GetStages())
            {
                if (stage.ID == objectiveId) { result = false; break; }
            }
            break;
        }
    }
    return result;*/
    int index = binarySearch(g_quest_list, 0, g_quest_list.size() - 1, refID);
    //g_quest_list[index].GetStages();
    if (index >= 0)
    {
        return atoi(g_quest_list[index].GetStageId().c_str()) < atoi(stageID.c_str());
    }
    return false;
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
    QSyncMessage message(msg);
    QSyncMessage replyALL; // Everyone except this client
    QSyncMessage replyClient;
    //QSyncMsgBody msgBody;
    std::cout << "Message received from " << client << "!" << std::endl;
    for (auto instruction : message.getMessages())
    {
        QSyncMsgBody msgAllBody;
        QSyncMsgBody msgClientBody;
        serverAction action = process_Message(client, instruction, &msgAllBody, &msgClientBody);
        if (msgAllBody.type != message_type::NONE) { replyALL.addMessage(msgAllBody); std::cout << "Have message for all..." << std::endl;
        }
        if (msgClientBody.type != message_type::NONE) { replyClient.addMessage(msgClientBody); std::cout << "Have message for this client!" << std::endl;
        }
        // Actions server side?
        switch (action)
        {
        case serverAction::NONE:
            break;
        case serverAction::SHUTDOWN:
            std::cout << "Received shutdown command." << std::endl;
            listener->Stop();
            break;
        default:
            break;
        }
    }

    if (replyALL.getSize() > 0)
    {
        std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
        if (replyALL.toString().size() > BUFFER_SIZE)
        {
            std::cout << "Buffer size exceeded! This will probably cause a crash!" << std::endl;
        }
        listener->Send_to_all(replyALL.toString(), &excluded);
    }
    if (replyClient.getSize() > 0)
    {
        if (replyClient.toString().size() > BUFFER_SIZE)
        {
            std::cout << "Buffer size exceeded! This will probably cause a crash!" << std::endl;
        }
        else
        {
            std::cout << "Sending messages back to client!" << std::endl;
        }
        listener->Send_to_specific(client, replyClient.toString());
    }
    else
    {
        std::cout << "No messages to return to client!" << std::endl;
    }
    std::vector<int> excluded = { client }; // Don't tell the one who told me about the update to update
    listener->Send_to_all(message.toString(), &excluded);
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

serverAction process_Message(int client, json message, QSyncMsgBody* replyAllBody, QSyncMsgBody* replyClientBody)
{
    message_type type;
    json body = message["Body"];
    /*std::string ID;
    std::string Name;
    std::string Stage = "";*/
    serverAction action = serverAction::NONE;

    message["Type"].get_to(type);
    switch (type)
    {
    case message_type::NEW_QUEST:
    {
        //std::cout << message.body << std::endl;
        //quest_and_objectives new_quest;
        std::string questString;
        body["Quest"].get_to(questString);
        QSyncQuest newQuest(questString);

        std::cout << client << ": New Quest - " << newQuest.GetRefId() << " " << newQuest.GetName() << " " << newQuest.GetStageId() << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), newQuest.GetRefId()) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        if (!is_new_quest(newQuest))
        {
            std::cout << "Quest has already been received, ignoring" << std::endl;
            break;
        }

        g_quest_list.push_back(newQuest); // TODO insert in order of refID
        std::sort(g_quest_list.begin(), g_quest_list.end()); // This should do the above

        replyAllBody->type = message_type::START_QUEST;
        replyAllBody->body = body;
    }
    break;
    case message_type::QUEST_UPDATED:
    {
        std::string questString;
        body["Quest"].get_to(questString);
        QSyncQuest quest(questString);

        std::cout << client << ": Quest Updated - " << quest.GetRefId() << " " << quest.GetName() << " " << quest.GetStageId() << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest.GetRefId()) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }

        if (is_new_quest(quest))
        {
            std::cout << "Quest is actually new, adding!" << std::endl;
            g_quest_list.push_back(quest); // TODO insert in order of refID
            std::sort(g_quest_list.begin(), g_quest_list.end()); // This should do the above
        }

        // Update Stage list for quest
        int index = binarySearch(g_quest_list, 0, g_quest_list.size() - 1, quest.GetRefId());
        std::vector<stage> trueStages = quest.GetStages();
        std::vector<stage> oldStages = g_quest_list[index].GetStages();
        // if any stages are completed on either client or server, count it as complted
        for (int i = 0; i > trueStages.size(); i++)
        {
            trueStages[i].Completed = trueStages[i].Completed || oldStages[i].Completed;
        }
        g_quest_list[index].SetStages(trueStages);

        body["Quest"] = g_quest_list[index].ToString();

        replyAllBody->type = message_type::UPDATE_QUEST;
        replyAllBody->body = body;
    }
    break;
    case message_type::QUEST_COMPLETED:
    {
        std::string questString;
        body["Quest"].get_to(questString);
        QSyncQuest quest(questString);

        std::cout << client << ": Quest Completed - " << quest.GetRefId() << " " << quest.GetName() << " " << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest.GetRefId()) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }

        int index = binarySearch(g_quest_list, 0, g_quest_list.size() - 1, quest.GetRefId());
        g_quest_list[index] = quest;

        replyAllBody->type = message_type::COMPLETE_QUEST;
        replyAllBody->body = body;
    }
    break;
    case message_type::QUEST_FAILED:
    {
        std::string questString;
        body["Quest"].get_to(questString);
        QSyncQuest quest(questString);

        std::cout << client << ": Quest Failed - " << quest.GetRefId() << " " << quest.GetName() << " " << quest.GetStageId() << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest.GetRefId()) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }

        int index = binarySearch(g_quest_list, 0, g_quest_list.size() - 1, quest.GetRefId());
        g_quest_list[index] = quest;

        replyAllBody->type = message_type::FAIL_QUEST;
        replyAllBody->body = body;
    }
    break;
    case message_type::REQUEST_ALL_QUEST_STATES: std::cout << "REQUEST_ALL_QUEST_STATES Not Implemented!" << std::endl; break;
    case message_type::REQUEST_CURRENT_QUESTS_COMPLETION:
    {
        std::cout << client << ": Wants currently active quests" << std::endl;
        json current_quests_info = json::array();
        for (auto quest : g_quest_list)
        {
            if (quest.IsActive() == false) { continue; } // Skip if inactive

            current_quests_info.push_back(quest.ToString());
        }
        std::cout << "Telling client about " << current_quests_info.size() << " new quests" << std::endl;
        //QSyncMsgB message(CURRENT_ACTIVE_QUESTS, current_quests_info.dump());
        replyClientBody->type = message_type::CURRENT_ACTIVE_QUESTS;
        replyClientBody->body = current_quests_info;
    }
    break;
    case message_type::RESEND_CONN_ACK: std::cout << "Ignoring RESEND_CONN_ACK..." << std::endl; break;

    case message_type::QUEST_INACTIVE:
    {
        std::string questString;
        body["Quest"].get_to(questString);
        QSyncQuest quest(questString);

        std::cout << client << ": Quest Inactive - " << quest.GetRefId() << " " << quest.GetName() << std::endl;
        if (std::find(g_questBlacklist.begin(), g_questBlacklist.end(), quest.GetRefId()) != g_questBlacklist.end()) // Don't do anything if the quest should be ignored
        {
            // Do nothing
            std::cout << "Quest is blacklisted, ignoring!" << std::endl;
            break;
        }
        replyAllBody->type = message_type::INACTIVE_QUEST;
        replyAllBody->body = body;
    }
    break;
    case message_type::OBJECTIVE_COMPLETED:
    {
        std::cout << client << ": Sent OBJECTIVE_COMPLETED but this is removed!";

        /*replyAllBody->type = message_type::COMPLETE_OBJECTIVE;
        replyAllBody->body = body;*/
    }
    break;
    case message_type::ACTIVE_QUESTS:
        // Client sent a list of their active quests, compare
    {
        std::cout << client << ": Sent list of their current quests, will process..." << std::endl;
        json quest_list = json::array();
        body.get_to(quest_list);
        /*int integer = quest_list.size();

        std::cout << body.is_array() << std::endl;
        std::cout << body.is_null() << std::endl;
        std::cout << body.is_string() << std::endl;

        std::cout << integer << std::endl;*/

        for (auto questJson : quest_list)
        {
            std::string questString;
            questJson["Quest"].get_to(questString);
            QSyncQuest quest(questString);

            std::cout << "Checking quest " << quest.GetRefId() << std::endl;
            if (is_new_quest(quest))
            {

                g_quest_list.push_back(quest); // TODO insert in order of refID
                std::sort(g_quest_list.begin(), g_quest_list.end()); // This should do the above

                replyAllBody->type = message_type::START_QUEST;
                replyAllBody->body = questJson;
                std::cout << "Added Quest " << quest.GetName() << " and told clients about it." << std::endl;
            }
  
            bool isDifferent = false;

                int index = binarySearch(g_quest_list, 0, g_quest_list.size() - 1, quest.GetRefId());
                std::vector<stage> trueStages = quest.GetStages();
                std::vector<stage> oldStages = g_quest_list[index].GetStages();
                // if any stages are completed on either client or server, count it as complted
                for (int i = 0; i > trueStages.size(); i++)
                {
                    if (trueStages[i].Completed != oldStages[i].Completed) { isDifferent = true; }
                    trueStages[i].Completed = trueStages[i].Completed || oldStages[i].Completed;
                }
                g_quest_list[index].SetStages(trueStages);

                questJson["Quest"] = g_quest_list[index].ToString();

                replyAllBody->type = message_type::UPDATE_QUEST;
                replyAllBody->body = questJson;
                std::cout << "Updated Stage/s for " << quest.GetRefId() << " " << quest.GetName() << " and told clients about it." << std::endl;
            
        }
    }
    break;
    case message_type::SHUTDOWN:
        action = serverAction::SHUTDOWN;
    break;
    default: break;
    }

    return action;
}