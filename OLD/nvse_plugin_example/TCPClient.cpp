#include "TCPClient.h"
#define BUFFER_SIZE 40000

TCPClient::TCPClient(std::string ipAddress, int port)
	: m_ipAddress(ipAddress), m_port(port)
{
}

TCPClient::~TCPClient()
{
	Cleanup(); // Makes sure it's ready to be destroyed
}
void TCPClient::SetServerIp(std::string ipAddress)
{
	this->m_ipAddress = ipAddress;
}
void TCPClient::SetServerPort(int port)
{
	this->m_port = port;
}
void TCPClient::send_message(std::string msg)
{
	send(sock, msg.c_str(), msg.size() + 1, 0);
}
/**
* Initializes the TCP Client
* @returns Success or failure as a boolean
*/
bool TCPClient::init()
{
	// Initialize winsock
	WSAData wsData;
	WORD ver = MAKEWORD(2, 2); // Version 2 of Winsock
	int wsOk = WSAStartup(ver, &wsData);

	if (wsOk != 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}
/**
* Connects to the TCP server specified when instantiating this object
* @returns The socket being used for communication
*/
SOCKET TCPClient::Connect()
{
	sock = socket(AF_INET, SOCK_STREAM, 0); // AF_INET: AF = Internet Family, INET = IPv4 (INET6 is for IPv6)
	if (sock != INVALID_SOCKET)
	{
		// Bind IP and Port to socket
		sockaddr_in hint;
		hint.sin_family = AF_INET;
		hint.sin_port = htons(m_port); // htons = Host TO Network Short
		//hint.sin_addr.S_un.S_addr = INADDR_ANY; // Could also use inet_piton to... bind to loopback
		inet_pton(AF_INET, m_ipAddress.c_str(), &hint.sin_addr); // Converts the string to a byte representation of the string in the desired format (AF_INET), also passed the address buffer to store it in

		// Now connect to server
		int connResult = connect(sock, (sockaddr*)&hint, sizeof(hint));
		if (connResult == SOCKET_ERROR)
		{
			std::cerr << "Can't connect to server, Err#" << WSAGetLastError() << std::endl;
			closesocket(sock);
			sock = INVALID_SOCKET;
			WSACleanup();
			return 0;
		}

		// Connected to server and all is well, can start receiving messages
		receive_messages_thread = std::thread(&TCPClient::ReceiveMessages_loop, this);
		receive_messages_thread.detach();
	}
	else { std::cerr << "Invalid Socket!" << std::endl; }

	return sock;
}
/**
* This function is called to properly clean up the object so it's ready to be destroyed
*/
void TCPClient::Cleanup()
{
	if (isConnected())
	{
		Disconnect();
	}
	WSACleanup();
}
/**
* Get the socket being used for communication. Does no check for if socket is open.
* @returns Socket used for communication
*/
SOCKET TCPClient::GetSocket()
{
	return this->sock;
}
/**
* Disconnectes from the TCP server. Only closes the socket and sets it to INVALID_SOCKET
*/
void TCPClient::Disconnect()
{
	if (isConnected())
	{
		closesocket(sock);
		sock = INVALID_SOCKET;

		receive_messages_thread.~thread();
	}
	//Cleanup();
}
/**
Checks to see if socket is set to INVALID_SOCKET, all other cases count as true
*/
bool TCPClient::isConnected()
{
	if (sock != INVALID_SOCKET) { return true; }
	else { return false; }
}
/**
* Gets all messages in the received messages queue and clears it
*/
std::list<std::string> TCPClient::GetMessages()
{
	std::lock_guard<std::mutex> guard(receive_messages_mutex); // Prevent a new message from being inserted while doing this
	std::list<std::string> output; // The full list of messages to be returned 
	output.swap(received_messages); // Swap the contents of the message queue and the return value, effectively clearing the incoming messages queue
	return output;
}

void TCPClient::ReceiveMessages_loop()
{
	char* buffer = new char[BUFFER_SIZE]; // Buffer to store incomming message
	//test = "yes.";
	while (sock != INVALID_SOCKET)
	{
		ZeroMemory(buffer, BUFFER_SIZE);
		int bytesReceived = recv(sock, buffer, BUFFER_SIZE, 0);
		//MessageReceived(this, std::string(buffer, 0, bytesReceived));
		std::lock_guard<std::mutex> guard(receive_messages_mutex);
		received_messages.push_back(std::string(buffer, 0, bytesReceived));
		//guard.unlock
		std::cout << "Received a message from the server at " << m_ipAddress << ", stored in receieved messages list" << std::endl;
	}
	//std::cout << "thread will die";
	delete[] buffer;
}
