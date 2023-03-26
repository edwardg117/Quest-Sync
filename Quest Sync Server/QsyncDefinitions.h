#pragma once
// This file contains a bunch of values I'm using to make my code easier to read
//#define CONNECTION_ACKNOWLEDGEMENT 0
//#define QUEST_UPDATED 1
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#define BUFFER_SIZE 40000

enum message_type {
	CONNECTION_ACKNOWLEDGEMENT,			// Server -- >Client: Client should not do anything until they receive this message
	QUEST_UPDATED,						// Client --> Sever: I progressed in a quest
	QUEST_COMPLETED,					// Client --> Server: I completed a quest
	UPDATE_QUEST,						// Server --> Client: Update this quest
	COMPLETE_QUEST,						// Server --> Client: Complete this quest
	REQUEST_ALL_QUEST_STATES,			// Client --> Server: Send through the state of every single quest (probably won't use these)
	ALL_QUEST_STATES,					// Server --> Client: Here's every single quest state
	REQUEST_CURRENT_QUESTS_COMPLETION,	// Client --> Server: Send through every single quest that is in progress
	CURRENT_ACTIVE_QUESTS,				// Server --> Client: Here's every quest that's in progress
	RESEND_CONN_ACK	,					// Client --> Server: Temp, thingy that tells the server to resend the connection acknowledgement in a bit.
	NEW_QUEST,							// Client --> Server: I just aquired a new quest
	QUEST_INACTIVE,						// Client --> Server: This quest was just removed from the active list but is not completed or failed
	QUEST_FAILED,						// Client --> Server: I failed this quest
	START_QUEST,						// Server --> Client: Start this quest at this stage
	FAIL_QUEST,							// Server --> Client: Mark this quest as failed
	INACTIVE_QUEST,						// Server --> Client: This quest is now inactive, remove it if you haven't already
	OBJECTIVE_COMPLETED,				// Client --> Server: I marked this objective as completed
	COMPLETE_OBJECTIVE,					// Server --> Client: Mark this objective as completed
	REQUEST_ACTIVE_QUESTS,				// Either --> Or: Send me your active list
	ACTIVE_QUESTS,						// Either --> Or: Here's my list of active stuff
	NONE								// Either --> Or: Message type not set
};

class QSyncMsgBody
{
public:
	QSyncMsgBody() {};
	QSyncMsgBody(message_type Type, json Body) : type(Type), body(Body) {};

	json body;
	message_type type = message_type::NONE;
};

class QSyncMessage
{
private:
	//std::vector<QSyncMsgBody> Body; // Messages the server should process
	json Body = json::array();
	UINT32 Size; // Number of elements in body
public:
	// Empty Constructor
	QSyncMessage() { Size = 0; } // Empty Constructor
	// Build from a sent message
	QSyncMessage(std::string msg) // Build from a sent message
	{
		fromString(msg);
	}

	void addMessage(QSyncMsgBody msg)
	{
		json message = {
			{"Body", msg.body},
			{"Type", msg.type}
		};
		
		Body.push_back(message);
		this->Size++;
	}
	json getMessages()
	{
		return this->Body;
	}

	UINT32 getSize()
	{
		return this->Size;
	}

	std::string toString()
	{
		json json_message = {
				{"Size", Size},
				{"Contents", Body}
		};
		return json_message.dump();
	}
	void fromString(std::string msg)
	{
		auto parsed_message = json::parse(msg);
		parsed_message["Size"].get_to(Size);
		parsed_message["Contents"].get_to(Body);
	}

};