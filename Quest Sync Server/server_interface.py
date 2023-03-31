import socket
import tkinter as tk
import json

"""
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
"""

MSG_DEF = {
    "CONNECTION_ACKNOWLEDGEMENT": 0,

    "Add Objective": 1,

    "Complete Quest": 2,

    "UPDATE_QUEST": 3,
    "COMPLETE_QUEST": 4,

    "Get Quest States": 5,

    "ALL_QUEST_STATES": 6,
    "REQUEST_CURRENT_QUESTS_COMPLETION": 7,
    "CURRENT_ACTIVE_QUESTS": 8,
    "RESEND_CONN_ACK": 9,

    "Start Quest": 10,

    "QUEST_INACTIVE": 11,

    "Fail Quest": 12,

    "START_QUEST": 13,
    "FAIL_QUEST": 14,
    "INACTIVE_QUEST": 15,

    "Complete Objective": 16,

    "COMPLETE_OBJECTIVE": 17,
    "REQUEST_ACTIVE_QUESTS": 18,
    "ACTIVE_QUESTS": 19,
    "NONE": 20,
    "SHUTDOWN": 21
}

class Application:
    def __init__(self):
        self.master = tk.Tk()
        self.input_windows = {}  # dictionary to store the input windows
        self.input_widgets = {}  # dictionary to store the input widgets

        # Create a socket object and connect to the server
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.connect(('localhost', 25575))

        # Create the UI elements
        window = self.master  # Create a Tkinter window
        side_panel = tk.Frame(window)  # Create a frame for the side panel
        self.input_fields = {  # Define the input fields for each button
            "Get Quest States" : [
            
            ],
            'Start Quest': [
                {'label': 'FormID', 'type': 'hex'},
                {'label': 'StageID', 'type': 'int'}
            ],
            "Add Objective": [
                {'label': 'FormID', 'type': 'hex'},
                {'label': 'StageID', 'type': 'int'}
            ],
            "Complete Objective": [
                {'label': 'FormID', 'type': 'hex'},
                {'label': 'StageID', 'type': 'int'}
            ],
            'Complete Quest': [
                {'label': 'FormID', 'type': 'hex'}
            ],
            "Fail Quest": [
                {'label': 'FormID', 'type': 'hex'}
            ],
            "Quit": []
        }

        # Create a button for each command
        for command, fields in self.input_fields.items():
            # Create a button with the command label
            if(command != "Get Quest States" and command != "Quit"):
                button = tk.Button(side_panel, text=command, command=lambda c=command, f=fields: self.create_input_window(c, f))
            elif(command == "Get Quest States"):
                button = tk.Button(side_panel, text=command, command=lambda c="Not Implemented!": print(c))
            elif(command == "Quit"):
                button = tk.Button(side_panel, text=command, command=window.destroy)
            # Add the button to the side panel
            button.pack(side=tk.TOP)

        # Create a text widget for displaying the output
        output_text = tk.Text(window)

        # Add the widgets to the window
        side_panel.pack(side=tk.LEFT, fill=tk.Y)
        # Create a scrollbar for the text widget
        output_scrollbar = tk.Scrollbar(window, command=output_text.yview)

        # Configure the text widget to use the scrollbar
        output_text.config(yscrollcommand=output_scrollbar.set)

        # Pack the scrollbar and text widget in the window
        output_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        output_text.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

    def run(self):
        self.master.mainloop()

    def create_input_window(self, command, fields):
        # Create the input window
        input_window = tk.Toplevel(self.master, name=command.lower())
        input_frame = tk.Frame(input_window)
        input_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        input_frame.master.title(command)
        self.input_widgets[command] = {}
        fields = self.input_fields[command]

        # Add each input widget to the dictionary
        for i, field in enumerate(fields):
            label = field['label']
            widget_type = field['type']
            # place label
            label_widget = tk.Label(input_frame, text=label)
            label_widget.grid(column=2*i, row=0)
            # Create the input widget
            if widget_type == 'hex':
                widget = tk.Entry(input_frame)
            elif widget_type == 'int':
                widget = tk.Entry(input_frame)
            else:
                print("errrrrrrrrrrrrrrrrrrrrrrrrrrrrr")
            widget.grid(column=2*i+ 1, row=0)
            
            #label_widget.pack(side=tk.TOP)
            #widget.pack(side=tk.TOP)
            self.input_widgets[command][label] = widget

        self.input_windows[command] = input_window
        #print(input_window)
        
        tk.Button(input_frame, text="Submit", command=lambda c=command: self.send_command(c)).grid(column=(len(fields)*2), row=1)
        tk.Button(input_frame, text="Cancel", command=input_window.destroy).grid(column=(len(fields)*2 + 1), row=1)

    def send_command(self, command):
        # Get the input widgets for this command
        widgets = self.input_widgets[command]

        # Get the values from each input widget
        complete = True
        values = {}
        for label, widget in widgets.items():
            value = widget.get()
            values[label] = value
            print(f"Found: {label} - {value}")
            if(value == ""): complete = False # No missing values

        # Build the command string using the input values
        # ...
        msg_body_body_json = {
            "ID":values["FormID"],
            "Stage": values.get("StageID", "NO STAGE ID FROM GUI"),
            "Name":"NO NAME FROM GUI"
        }
        msg_body_json = {
            "Type": MSG_DEF[command],
            "Body": msg_body_body_json
        }
        msg_json = {
            "Size":1,
            "Contents": [msg_body_json]
        }
        #json.dumps(msg_body_json)
        if(complete):
            print(f"Going to send: {json.dumps(msg_json)}")

            self.client_socket.sendall(json.dumps(msg_json).encode())

            window = self.input_windows[command]
            window.destroy() # Close the window
        else:
            print("Can't send incomplete!")

        # Send the command to the server and display the output
        # ...

App = Application()
App.run()
