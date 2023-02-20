#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <list>
#include <algorithm>
#include <WS2tcpip.h>				// Header file for winsock functions
#pragma comment (lib, "ws2_32.lib")	// Winsock library file



class TCPClient
{
public:
	TCPClient(std::string ipAddress, int port);
	~TCPClient();
	void SetServerIp(std::string ipAddress);
	void SetServerPort(int port);
	void send_message(std::string msg);
	bool init();								// Calls the nesaccary setup functions for Winsock
	SOCKET Connect();							// Makes the connection to the server and starts a daemon thread (ReceiveMessages_loop()) to constantly record new messages without halting the main thread
	bool isConnected();							// Simple check to see if sock is not INVALID_SOCKET
	SOCKET GetSocket();							// Returns the value of sock, just incase I want to grab it directly or something
	std::list<std::string> GetMessages();		// Returns the list of all new messages since the last time this function was called
	void Disconnect();							// Disconnects from the server and stops the daemon thread (ReceiveMessages_loop())
	void Cleanup();								// Calls Disconnect() and then runs cleanup for Winsock

private:
	std::string m_ipAddress;					// Ip Address to connect to
	int m_port;									// Port to use
	SOCKET sock = INVALID_SOCKET;				// The socket that's connected to the server
	std::thread receive_messages_thread;		// The thread that is running the loop to receive messages and store them in received_messages
	std::mutex receive_messages_mutex;			// The mutex used to lock the received_messages list to prevent race condition when program is trying to access them and server is sending a message
	std::list<std::string> received_messages;	// A list of messages sent from the server in chronologial order

	void ReceiveMessages_loop();				// The function used in the daemon thread that continuously receives and records new messages
};

