#pragma once
// This file contains a bunch of values I'm using to make my code easier to read
//#define CONNECTION_ACKNOWLEDGEMENT 0
//#define QUEST_UPDATED 1
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
	RESEND_CONN_ACK						// Client --> Server: Temp, thingy that tells the server to resend the connection acknowledgement in a bit.
};