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
	RESEND_CONN_ACK,					// Client --> Server: Temp, thingy that tells the server to resend the connection acknowledgement in a bit.
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
	NONE,								// Either --> Or: Message type not set
	SHUTDOWN							// Interface --> Server: Stop oeprations
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

/// <summary>
/// Used to compare client and server versions during initial connection.
/// This information is sent to the client, who will decide if they support this version.
/// </summary>
class QSyncVersion
{
public:
	QSyncVersion()
	{
	}
	/// <summary>
	/// Builds object from a message.
	/// </summary>
	/// <param name="msg">Sould be the Body from a QsyncMessage from the server with the CONN_ACK</param>
	QSyncVersion(std::string msg)
	{
		fromString(msg);
	}

	void SetServerVersion(const int serverVersion[2])
	{
		this->ServerVersion[0] = serverVersion[0];
		this->ServerVersion[1] = serverVersion[1];
	}
	void SetMinVersion(const int minVersion[2])
	{
		this->FromVersion[0] = minVersion[0];
		this->FromVersion[1] = minVersion[1];
	}
	void SetMaxVersion(const int maxVersion[2])
	{
		this->ToVersion[0] = maxVersion[0];
		this->ToVersion[1] = maxVersion[1];
	}
	/// <summary>
	/// Converts object to a string for sending
	/// </summary>
	/// <returns></returns>
	std::string toString()
	{
		json json_message = {
			{"ServerVersion", ServerVersion},
			{"FromVersion", FromVersion},
			{"ToVersion", ToVersion}
		};
		return json_message.dump();
	}

	/// <summary>
	/// Overwrites the info stored in this object based on what is in the message from the server.
	/// </summary>
	/// <param name="msg">Sould be the Body from a QsyncMessage from the server with the CONN_ACK</param>
	void fromString(std::string msg)
	{
		auto parsed_message = json::parse(msg);
		parsed_message["ServerVersion"].get_to(ServerVersion);
		parsed_message["FromVersion"].get_to(FromVersion);
		parsed_message["ToVersion"].get_to(ToVersion);
	}

	/// <summary>
	/// Determines if the server thinks it's compatible with this client version.
	/// </summary>
	/// <param name="ClientMajorVersion">Major version of the client</param>
	/// <param name="ClientMinorVersion">Minor version of the client</param>
	/// <returns>If server expects to be compatible</returns>
	bool isVersionCompatible(int ClientMajorVersion, int ClientMinorVersion)
	{
		bool verdict = false;
		if (ClientMajorVersion >= FromVersion[0] && ClientMajorVersion <= ToVersion[0])
		{
			if (ClientMajorVersion == ToVersion[0])
			{
				if (ClientMinorVersion <= ToVersion[1])
				{
					if (ClientMajorVersion == FromVersion[0])
					{
						if (ClientMinorVersion >= FromVersion[1])
						{
							verdict = true; // The min and max Major version is the same, need to make sure it's greater than or equal to the minor version too
						}
					}
					else {
						verdict = true; // Client is at the max supported version and does not exceed the minor veresion
					}
				}
			}
			else if (ClientMajorVersion == FromVersion[0])
			{
				if (ClientMinorVersion >= FromVersion[1])
				{
					verdict = true; // Client is at the min supported version and is not lower than the min minor version
				}
			}
			else
			{
				verdict = true; // Client is between the min and max versions, minor versions should not matter in this case
			}
		}

		return verdict;
	}
private:
	// Server version [majorVersion, minorVersion]
	int ServerVersion[2];
	// The minimum version of the Client version that the server expects to support (Ultimately if the client thinks it can support the server version, this is ignored)
	int FromVersion[2];
	// The maximum version of the Client version that the server expects to support (Ultimately if the client thinks it can support the server version, this is ignored)
	int ToVersion[2];
};

struct stage {
	UINT8 ID;
	bool Completed;
};

/// <summary>
/// Used to define how a quest is transferred between the server and client.
/// </summary>
class QSyncQuest
{
public:
	QSyncQuest() {}
	QSyncQuest(std::string RefId, std::string StageId, std::vector<stage> Stages)
		: RefId(RefId), StageId(StageId), Stages(Stages) {}
	QSyncQuest(std::string Name, std::string RefId, std::string StageId, std::vector<stage> Stages)
		: Name(Name), RefId(RefId), StageId(StageId), Stages(Stages) {}
	QSyncQuest(std::string RefId, std::string StageId, std::vector<stage> Stages, bool Active, bool Completed, bool Failed)
		: RefId(RefId), StageId(StageId), Stages(Stages), Active(Active), Completed(Completed), Failed(Failed) {}
	QSyncQuest(std::string Name, std::string RefId, std::string StageId, std::vector<stage> Stages, bool Active, bool Completed, bool Failed)
		: Name(Name), RefId(RefId), StageId(StageId), Stages(Stages), Active(Active), Completed(Completed), Failed(Failed) {}
	QSyncQuest(std::string msg)
	{
		FromString(msg);
	}

	std::string ToString()
	{
		json stages_json = json::array();
		for (stage qstage : Stages)
		{
			stages_json.push_back({ {"ID",qstage.ID},{"C",qstage.Completed} });
		}
		json message = {
			{"RefId", RefId},
			{"StageId", StageId},
			{"Stages", stages_json}
		};
		if (Name.length() > 0) {
			message.push_back({ "Name", Name });
		}
		if (Active != NULL) {
			message.push_back({ "A", Active });
		}
		if (Completed != NULL) {
			message.push_back({ "C", Completed });
		}
		if (Failed != NULL) {
			message.push_back({ "F", Failed });
		}
		return message.dump();
	}

	void FromString(std::string msg)
	{
		json stages_json = json::array();
		std::vector<stage> stages;
		auto parsed_message = json::parse(msg);
		parsed_message["RefId"].get_to(RefId);
		parsed_message["StageId"].get_to(StageId);
		parsed_message["Stages"].get_to(stages_json);
		for (json stage_json : stages_json)
		{
			stage stage;
			stage_json["ID"].get_to(stage.ID);
			stage_json["C"].get_to(stage.Completed);
			stages.push_back(stage);
		}
		this->Stages = stages;
		if (parsed_message.contains("Name")) {
			parsed_message["Name"].get_to(Name);
		}
		if (parsed_message.contains("A")) {
			parsed_message["A"].get_to(Active);
		}
		if (parsed_message.contains("C")) {
			parsed_message["C"].get_to(Completed);
		}
		if (parsed_message.contains("F")) {
			parsed_message["F"].get_to(Failed);
		}
	}

	std::string GetName() {
		return Name;
	}
	std::string GetRefId() {
		return RefId;
	}
	std::string GetStageId() {
		return StageId;
	}
	std::vector<stage> GetStages() {
		return Stages;
	}
	
	bool IsActive() {
		return Active;
	}
	bool IsCompleted() {
		return Completed;
	}
	bool isFailed() {
		return Failed;
	}

	void SetName(std::string Name) {
		this->Name = Name;
	}
	void SetRefId(std::string RefId) {
		this->RefId = RefId;
	}
	void SetStageId(std::string StageId) {
		this->StageId = StageId;
	}
	void SetStages(std::vector<stage> Stages) {
		this->Stages = Stages;
	}

	bool operator<(const QSyncQuest& other) const
	{
		return RefId < other.RefId;
	}
	bool operator<(const std::string& other) const
	{
		return RefId < other;
	}
	bool operator>(const QSyncQuest& other) const
	{
		return RefId > other.RefId;
	}
	bool operator>(const std::string& other) const
	{
		return RefId > other;
	}
	bool operator==(const QSyncQuest& other) const
	{
		return RefId == other.RefId;
	}
	bool operator==(const std::string& other) const
	{
		return RefId == other;
	}

private:
	int i = 0;
	std::string Name;
	std::string RefId;
	std::string StageId;
	bool Active;
	bool Completed;
	bool Failed;
	std::vector<stage> Stages;
};