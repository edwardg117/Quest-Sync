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
// This function runs whenever a client sends a message to the server
void Listener_MessageReceived(TCPListener* listener, int client, std::string msg)
{ // Do server logic here
    //std::cout << "I got a message!";
    // 
    //listener->Send_to_specific(client, msg);

    //std::vector<int> excludeVector = { client };
    //listener->Send_to_all(msg, &excludeVector);

    //listener->Send_to_all(msg);
    auto parsed_message = json::parse(msg);
    message_type m_type;
    parsed_message["message_type"].get_to(m_type);

    switch (m_type)
    {
    case message_type::QUEST_COMPLETED: break;
    case message_type::QUEST_UPDATED: break;
    case message_type::REQUEST_ALL_QUEST_STATES: break;
    case message_type::REQUEST_CURRENT_QUESTS_COMPLETION: break;
    case message_type::RESEND_CONN_ACK:
    {
        int itime;
        //std::string stime;
        parsed_message["message_contents"].get_to(itime);
        //itime = stoi(stime);
        Sleep(itime * (1000));
        json json_message = {
            {"message_type", message_type::RESEND_CONN_ACK},
            {"message_contents", ""}
        };
        listener->Send_to_specific(client, json_message.dump());
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