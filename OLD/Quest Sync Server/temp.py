import socket
import tkinter as tk


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
            'Start Quest': [
                {'label': 'FormID', 'type': 'hex'},
                {'label': 'StageID', 'type': 'int'}
            ],
            'Complete Quest': [
                {'label': 'FormID', 'type': 'hex'}
            ]
        }

        # Create a button for each command
        for command, fields in self.input_fields.items():
            # Create a button with the command label
            button = tk.Button(side_panel, text=command, command=lambda c=command, f=fields: self.create_input_window(c, f))

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
        input_window = tk.Toplevel(self.master)
        input_frame = tk.Frame(input_window, name=command)
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

        tk.Button(input_frame, text="Submit").grid(column=(len(fields)*2), row=1)
        tk.Button(input_frame, text="Cancel", command=input_window.destroy).grid(column=(len(fields)*2 + 1), row=1)

    def send_command(self, command):
        # Get the input widgets for this command
        widgets = self.input_widgets[command]

        # Get the values from each input widget
        values = {}
        for label, widget in widgets.items():
            value = widget.get()
            values[label] = value

        # Build the command string using the input values
        # ...

        # Send the command to the server and display the output
        # ...

App = Application()
App.run()
