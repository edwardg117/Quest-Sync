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
	REQUEST_CURRENT_QUESTS_COMPLETION,	// Client --> Server: Send through every single quest that has been started or completed
	CURRENT_QUESTS_COMPLETEION,			// Server --> Client: Here's every quest that's been started or completed
	RESEND_CONN_ACK	,					// Client --> Server: Temp, thingy that tells the server to resend the connection acknowledgement in a bit.
	NEW_QUEST,							// Client --> Server: I just aquired a new quest
	QUEST_INACTIVE,						// Client --> Server: This quest was just removed from the active list but is not completed or failed
	QUEST_FAILED,						// Client --> Server: I failed this quest
	START_QUEST,						// Server --> Client: Start this quest at this stage
	FAIL_QUEST,							// Server --> Client: Mark this quest as failed
	INACTIVE_QUEST						// Server --> Client: This quest is now inactive, remove it if you haven't already
};

class QSyncMessage
{
public:
	// Empty Constructor
	QSyncMessage() {} // Empty Constructor
	// Build from a sent message
	QSyncMessage(std::string msg) // Build from a sent message
	{
		fromString(msg);
	}
	// Build message to send
	QSyncMessage(message_type type, std::string body) // Build message to send
		: type(type), body(body) {}
	std::string body;
	message_type type;

	std::string toString()
	{
		json json_message = {
				{"message_type", type},
				{"message_contents", body}
		};
		return json_message.dump();
	}
	void fromString(std::string msg)
	{
		auto parsed_message = json::parse(msg);
		parsed_message["message_type"].get_to(type);
		parsed_message["message_contents"].get_to(body);
	}
};