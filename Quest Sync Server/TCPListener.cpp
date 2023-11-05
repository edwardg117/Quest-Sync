#include "TCPListener.h"
#include "Version.h"
#include "QsyncDefinitions.h"

SOCKET TCPListener::CreateSocket()
{
	SOCKET listener = socket(AF_INET, SOCK_STREAM, 0); // AF_INET: AF = Internet Family, INET = IPv4 (INET6 is for IPv6)
	if (listener != INVALID_SOCKET)
	{
		// Bind IP and Port to socket
		sockaddr_in hint;
		hint.sin_family = AF_INET;
		hint.sin_port = htons(m_port); // htons = Host TO Network Short
		if (m_ipAddress == "")
		{
			hint.sin_addr.S_un.S_addr = INADDR_ANY;
		}// Could also use inet_piton to... bind to loopback
		else
		{
			inet_pton(AF_INET, m_ipAddress.c_str(), &hint.sin_addr);
		} // Converts the string to a byte representation of the string in the desired format (AF_INET), also passed the address buffer to store it in

		// Now bind to port and ip
		int bindOk = bind(listener, (sockaddr*)&hint, sizeof(hint));
		if (bindOk != SOCKET_ERROR)
		{
			//Successfully bound, start listening
			int listenOk = listen(listener, SOMAXCONN);
			if (listenOk == SOCKET_ERROR)
			{
				return -1;
			}
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
	return listener;
}

// Waits for a connection froma client and accepts it
SOCKET TCPListener::WaitForConnection(SOCKET listening)
{
	SOCKET client = accept(listening, nullptr, nullptr);
	return client;
}

TCPListener::TCPListener(std::string ipAddress, int port, MessageReceivedHandler handler)
	: m_ipAddress(ipAddress), m_port(port), MessageReceived(handler)
{
	this->running = false;
}

TCPListener::~TCPListener()
{
	Cleanup(); // Cleanup just in case someone destroys this without running cleanup.
}

void TCPListener::Send_to_specific(int clientSocket, std::string msg)
{
	send(clientSocket, msg.c_str(), msg.size() + 1, 0);
}
/**
* Sends a message to all connected clients. Optionally exclude clients based on socket id.
*
* @param msg A string to send to the clients
* @param excludeSockets *optional* A vector of Socket ids to exclude
*/
void TCPListener::Send_to_all(std::string msg, std::vector<int>* excludeSockets)
{
	/*fd_set copy = master; // Create a copy of the master set
	int socketCount = master.fd_count;
	std::cout << "Socket Count: " << socketCount << std::endl;
	*/
	int socketCount = conected_clients.size();
	for (int i = 0; i < socketCount; i++) // Loop over all connected sockets
	{
		SOCKET sock = conected_clients[i].GetSocketId(); // Curret socket
		// Now check to see if this socket is excluded
		//std::cout << "Current Socket: " << sock << " " << i << "/" << socketCount << std::endl;
		bool will_send = true;
		if (excludeSockets != nullptr)
		{
			//std::cout << excludeSockets->size() << std::endl;
			for (int j = 0; j < excludeSockets->size(); j++) // Loop over all sockets to exclude
			{
				//std::cout << "Checking Socket: " << sock << " " << j << "/" << excludeSockets->size() << std::endl;
				if (sock == (*excludeSockets)[j])
				{
					//std::cout << sock << " will be ignored!" << std::endl;
					will_send = false;
					break;
				}
				else
				{
					//std::cout << "I will not be ignored!" << std::endl;
				}
			}
		}
		else {
			//std::cout << "No sockets to exclude" << std::endl; 
		}
		// Only send if supposed to
		if (will_send)
		{
			//std::cout << "Sending to " << sock << std::endl;
			send(sock, msg.c_str(), msg.size() + 1, 0);
		}
		else
		{
			//std::cout << "Not sending to " << sock << std::endl;
		}

	}
}

bool TCPListener::init()
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

void TCPListener::Run()
{
	// Create a listening socket
	SOCKET listening = CreateSocket();
	char* buffer = new char[BUFFER_SIZE];
	//fd_set master; // Master set of connections
	FD_ZERO(&master); // Clear and initialize the set
	FD_SET(listening, &master);

	if (listening == INVALID_SOCKET)
	{
		std::cout << "Could not bind to socket!" << std::endl;
		std::cout << WSAGetLastError() << std::endl;
		return;
	}
	this->running = true;

	while (this->running)
	{
		if (listening == INVALID_SOCKET)
		{
			std::cout << "Could not bind to socket!" << std::endl;
			std::cout << WSAGetLastError() << std::endl;
			return;
		}

		fd_set copy = master; // Create a copy of the master set
		int socketCount = select(0, &copy, nullptr, nullptr, nullptr);
		for (int i = 0; i < socketCount; i++)
		{
			SOCKET sock = copy.fd_array[i];
			if (sock == listening)
			{
				// A new connection is waiting, accept it.
				sockaddr_in client_info;
				int clientsize = sizeof(client_info);
				// Accept new connection
				SOCKET client = accept(listening, (sockaddr*)&client_info, &clientsize);
				// Add new connection to the list of connected clients
				FD_SET(client, &master);
				conected_clients.push_back(Client(client, client_info, clientsize));


				char host[NI_MAXHOST];
				char service[NI_MAXSERV];
				ZeroMemory(host, NI_MAXHOST); // memset for linux? "memset(host, 0, NI_MAXHOST);
				ZeroMemory(service, NI_MAXSERV); // memset for linux? "memset(service, 0, NI_MAXHOST);
				if (getnameinfo((sockaddr*)&client, sizeof(client_info), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
				{
					std::cout << host << " connected on port " << service << std::endl;
				}
				else
				{
					// DNS lookup?
					inet_ntop(AF_INET, &client_info.sin_addr, host, NI_MAXHOST);
					std::cout << host << " connected on port " << ntohs(client_info.sin_port) << std::endl;
				}
				std::cout << "Client Connected: " << client << std::endl;
				QSyncVersion versionInfo;
				versionInfo.SetServerVersion(Version::ServerVersion);
				versionInfo.SetMinVersion(Version::FromVersion);
				versionInfo.SetMaxVersion(Version::ToVersion);
				QSyncMessage message;
				QSyncMsgBody body;
				body.type = message_type::CONNECTION_ACKNOWLEDGEMENT;
				json jbody = {
					{"message", versionInfo.toString()}
				};
				body.body = jbody;
				message.addMessage(body);
				std::cout << "Sending Connection Acknowledgment. " << message.toString() << std::endl;
				Send_to_specific(client, message.toString());
				std::cout << "Sent!" << std::endl;
			}
			else
			{
				// A message from an already connected client, receive it
				ZeroMemory(buffer, BUFFER_SIZE); // Explore memset for linux compat?
				// Receive data from client
				int bytesIn = recv(sock, buffer, BUFFER_SIZE, 0);
				if (bytesIn <= 0)
				{
					// Client has disconnected, drop client
					std::cout << sock << " has left." << std::endl;
					// Cleanup
					closesocket(sock);
					FD_CLR(sock, &master);
					for (int i = 0; i < conected_clients.size(); i++) // Remove from connected client list too
					{
						if (sock == conected_clients[i].GetSocketId())
						{
							conected_clients.erase(conected_clients.begin() + i);
						}
					}
				}
				else if (bytesIn > 0)
				{
					if (MessageReceived != NULL)
					{
						MessageReceived(this, sock, std::string(buffer, 0, bytesIn)); // Message is consructed with the buffer, the index to strart from and the size of the message to copy (start + size = total length to copy)
					}
				}
			}
		}
	}
	delete[] buffer;
}

void TCPListener::Stop()
{
	// Message all clients to disconnect
	//Send_to_all();
	// Stop Listening
	this->running = false;
	// Run Cleanup
	this->Cleanup();
}

void TCPListener::Cleanup()
{
	conected_clients.clear();
	WSACleanup();
}

Client::Client(SOCKET client, sockaddr_in client_info, int client_size)
	: socket(client), client_info(client_info), client_size(client)
{
}

std::string Client::GetIpAddress()
{
	char host[NI_MAXHOST];
	char service[NI_MAXSERV];
	ZeroMemory(host, NI_MAXHOST); // memset for linux? "memset(host, 0, NI_MAXHOST);
	ZeroMemory(service, NI_MAXSERV); // memset for linux? "memset(service, 0, NI_MAXHOST);
	if (getnameinfo((sockaddr*)&socket, sizeof(client_info), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
	{
		//std::cout << host << " connected on port " << service << std::endl;
	}
	else
	{
		// DNS lookup?
		inet_ntop(AF_INET, &client_info.sin_addr, host, NI_MAXHOST);
		//std::cout << host << " connected on port " << ntohs(client_info.sin_port) << std::endl;
	}
	return std::string(host);
}

std::string Client::GetPort()
{
	std::string port;
	char host[NI_MAXHOST];
	char service[NI_MAXSERV];
	ZeroMemory(host, NI_MAXHOST); // memset for linux? "memset(host, 0, NI_MAXHOST);
	ZeroMemory(service, NI_MAXSERV); // memset for linux? "memset(service, 0, NI_MAXHOST);
	if (getnameinfo((sockaddr*)&socket, sizeof(client_info), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
	{
		//std::cout << host << " connected on port " << service << std::endl;
		port = std::string(service);
	}
	else
	{
		// DNS lookup?
		inet_ntop(AF_INET, &client_info.sin_addr, host, NI_MAXHOST);
		//std::cout << host << " connected on port " << ntohs(client_info.sin_port) << std::endl;
		port = std::to_string(ntohs(client_info.sin_port));
	}
	return port;
}

SOCKET Client::GetSocketId()
{
	return socket;
}
