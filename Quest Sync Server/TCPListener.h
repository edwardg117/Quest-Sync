#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <WS2tcpip.h>				// Header file for winsock functions
#pragma comment (lib, "ws2_32.lib")	// Winsock library file
//#define BUFFER_SIZE 40000
#include "QsyncDefinitions.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Forward declaration
class TCPListener;
class Client;
// Callback to data received
typedef void(*MessageReceivedHandler)(TCPListener* listener, int socketId, std::string msg);

class TCPListener
{
public:
	TCPListener(std::string ipAddress, int port, MessageReceivedHandler handler);
	~TCPListener();
	// Send a message to the specified client
	void Send_to_specific(int clientSocket, std::string msg);
	void Send_to_all(std::string msg, std::vector<int>* excludeSockets = nullptr);
	// Initialize winsock
	bool init();
	// The main procesing loop
	void Run();
	// Cleanup after using the service
	void Cleanup();

private:
	std::string m_ipAddress;
	int m_port;
	MessageReceivedHandler MessageReceived;
	fd_set master; // Master set of connections
	//std::vector<SOCKET> master; //Master list of connections
	std::vector<Client> conected_clients;

	// Create a socket
	SOCKET CreateSocket();
	// Wait for a connection
	SOCKET WaitForConnection(SOCKET listening);



};

class Client
{
public:
	Client(SOCKET client, sockaddr_in client_info, int client_size);
	std::string GetIpAddress();
	std::string GetPort();
	SOCKET GetSocketId();
private:
	SOCKET socket;
	sockaddr_in client_info;
	int client_size;
};
