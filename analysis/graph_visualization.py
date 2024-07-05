import re

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.collections import PathCollection
from matplotlib.lines import Line2D

from datetime import datetime, timedelta
from enum import Enum, auto
from typing import List

use_remote_server_logs = True
remote_server_time_offset_hours = 4;
remote_server_time_offset_seconds = 6.921; 

class Client(Enum):
    received_unique_id = auto()
    render_start = auto()
    render_complete = auto()
    prediction_deltas_recorded = auto()
    received_game_state = auto()
    sending_input_snapshot = auto()
    recorded_input_into_history = auto()
    physics_tick = auto() 
    just_reconciled = auto()
class Server(Enum):
    received_input_snapshot = auto()
    sending_game_state = auto()
    physics_tick = auto()
    general_tick = auto() # general information about the entire server tick
    updated_player_state_with_input_snapshot = auto()

class LogType:
    irrelevant = -999
    client = Client
    server = Server

# program crashes if the log files contain a log type which is not accounted for here
log_type_to_y_position = {
    LogType.client.recorded_input_into_history: 5,
    LogType.client.render_start: 4.5,
    LogType.client.render_complete: 4.25,
    LogType.client.physics_tick: 4,
    LogType.client.prediction_deltas_recorded: 3.5,
    LogType.client.just_reconciled: 3,
    LogType.client.received_game_state: 2,
    LogType.client.sending_input_snapshot: 1.5,
    LogType.client.received_unique_id: 0,

    LogType.server.sending_game_state: -1, 
    LogType.server.received_input_snapshot: -2, 
    LogType.server.updated_player_state_with_input_snapshot: -3, 
    LogType.server.physics_tick: -4, 
    LogType.server.general_tick: -4.5, 
}

log_type_to_plt = {}
currently_visible_log_types = set()

def server_message_to_log_type(message: str):
    if "just updated player velocity" in message:
        return LogType.server.updated_player_state_with_input_snapshot
    elif "Just received input snapshot" in message:
        return LogType.server.received_input_snapshot
    elif "Sending game update" in message:
        return LogType.server.sending_game_state
    elif "physics tick with delta" in message:
        return LogType.server.physics_tick
    elif "started server tick" in message:
        return LogType.server.general_tick
    else:
        return LogType.irrelevant

def client_message_to_log_type(message: str):
    if "Received unique ID from server" in message:
        return LogType.client.received_unique_id
    elif "starting render" in message:
        return LogType.client.render_start    
    elif "render complete" in message:
        return LogType.client.render_complete    
    elif "Just received a game update" in message:
        return LogType.client.received_game_state    
    elif "sending input snapshot" in message:
        return LogType.client.sending_input_snapshot
    elif "inserting into input snapshot" in message:
        return LogType.client.recorded_input_into_history 
    elif "physics tick with delta" in message:
        return LogType.client.physics_tick
    elif "prediction deltas" in message:
        return LogType.client.prediction_deltas_recorded
    elif "starting reconciliation," in message:
        return LogType.client.just_reconciled
    else:
        return LogType.irrelevant


def replace_commas_with_newlines(input_string):
    return input_string.replace(", ", "\n")


# Function to parse log data and sort into respective arrays
def parse_log_data_to_dict_of_log_type_to_tuples_of_time_to_message(client_log_data, server_log_data):

    log_type_to_logs = {}

    client_and_server_log_data = [(True, client_log_data), (False, server_log_data)]

    for client_and_server_log_datum in client_and_server_log_data:
        for_client, log_data = client_and_server_log_datum
        for_server = not for_client

        for log in log_data:
            timestamp_str = log.split(']')[0][1:-3]  # Remove nanoseconds
            if timestamp_str == '':
                print('empty one', log)
            timestamp = datetime.strptime(timestamp_str, '%Y-%m-%d %H:%M:%S.%f')

            if for_server and use_remote_server_logs:
                timestamp = timestamp - timedelta(hours=remote_server_time_offset_hours, seconds=remote_server_time_offset_seconds)

            message = log.strip()  # Remove leading/trailing whitespaces

            # message = replace_commas_with_newlines(message)

            if for_client:
                log_type = client_message_to_log_type(message)
            else:
                log_type = server_message_to_log_type(message)

            if log_type != LogType.irrelevant:
                currently_visible_log_types.add(log_type)
                if log_type not in log_type_to_logs:
                    log_type_to_logs[log_type] = []

                y_offset = get_y_offset(log_type, message)

                log_type_to_logs[log_type].append(((timestamp, log_type_to_y_position[log_type] + y_offset), message))

    return log_type_to_logs

def get_y_offset(log_type, message) -> float:
    """a function which returns a y offset for the point on the graph for the given log message"""
    offset = 0
    if log_type == LogType.client.prediction_deltas_recorded:
        val = extract_poslen(message)
        if val == None:
            offset = 0
        else:
            offset = float(val)
    return offset

def extract_poslen(log_message):
    # Regex pattern to match 'poslen:' followed by a number (integer, floating-point, or scientific notation)
    pattern = r"poslen:\s(\d+(\.\d+)?([eE][+-]?\d+)?)"
    # Search for the pattern in the string
    match = re.search(pattern, log_message)
    # Extract the number after 'poslen:'
    if match:
        return float(match.group(1))
    else:
        return None

class LineTracker:
    def __init__(self, ax):
        self.ax = ax
        self.lines = []

    def add_line(self, x1, y1, x2, y2, color='blue', linestyle='-', linewidth=1):
        line, = self.ax.plot([x1, x2], [y1, y2], color=color, linestyle=linestyle, linewidth=linewidth)
        self.lines.append(line)

    def remove_line(self, index):
        if 0 <= index < len(self.lines):
            line = self.lines.pop(index)
            line.remove()
        else:
            print("Invalid index")

    def clear_all_lines(self):
        for line in self.lines:
            line.remove()
        self.lines = []

class GraphInteraction:
    def __init__(self, ax, log_type_to_logs, annotation, fig):
        self.ax = ax
        self.log_type_to_logs = log_type_to_logs
        self.annotation = annotation
        self.xy_to_pinned_annotation = {}
        self.vlines = []  # List to keep track of vertical lines
        self.annotation_lines = LineTracker(ax)
        self.fig = fig
        self.left_mouse_pressed = False
        self.right_mouse_pressed = False
        self.left_shift_pressed = False


    def clear_pinned_annotations(self):
        print(f'clearing {len(self.xy_to_pinned_annotation)} annotations')
        for annotation in self.xy_to_pinned_annotation.values():
            annotation.remove()
        self.xy_to_pinned_annotation = {}
        self.fig.canvas.draw_idle()

    def on_key_press(self, event):
        if event.key == 'shift':
            self.left_shift_pressed = True

    # Function to handle key release events
    def on_key_release(self, event):
        if event.key == 'shift':
            self.left_shift_pressed = False

    def set_mouse_pressed(self, event, status: bool):
        left_mouse_code, right_mouse_code = 1, 3
        if event.button == left_mouse_code:
            self.left_mouse_pressed = status
            self.markup_diagram(event)

        if event.button == right_mouse_code: # clear annotations
            self.right_mouse_pressed = status
            if self.left_mouse_pressed:
                self.annotation_lines.clear_all_lines()
            else:
                self.clear_pinned_annotations()


    def hover(self, event):
        self.markup_diagram(event)

    def markup_diagram(self, event):
        if event.inaxes == self.ax:
            mouse_event_not_in_any_sub_plot = True
            for log_type in currently_visible_log_types:
                log_plt = log_type_to_plt[log_type] 
                mouse_event_occurs_in_this_plt, ind = log_plt.contains(event) 
                # A dictionary {'ind': pointlist}, where pointlist is a list of points of the line that are within the pickradius around the event position.
                if mouse_event_occurs_in_this_plt: # this is empty if nothing nearby
                    log_data = self.log_type_to_logs[log_type]
                    messages = [t[1] for t in log_data]
                    pinned_annotation_requested = self.left_mouse_pressed and not self.left_shift_pressed
                    self.update_annot(log_plt, self.annotation, ind, messages, pinned_annotation_requested)
                    mouse_event_not_in_any_sub_plot = False
                    self.handle_drawing_of_vertical_lines(event, log_plt, ind)

                    if log_type == LogType.client.physics_tick and self.left_mouse_pressed and self.left_shift_pressed:
                        self.draw_input_snapshot_client_to_server_processed_line(log_data, log_plt, ind)

                    if log_type == LogType.client.received_game_state and self.left_mouse_pressed and self.left_shift_pressed:
                        # self.draw_input_snapshot_server_send_to_client_receive_game_state_line(log_data, log_plt, ind)
                        self.draw_line_from_mouse_that_pressed_on_client_or_server_receive_to_the_corresponding_send(False, log_data, log_plt, ind)

                    if log_type == LogType.server.received_input_snapshot and self.left_mouse_pressed and self.left_shift_pressed:
                        self.draw_line_from_mouse_that_pressed_on_client_or_server_receive_to_the_corresponding_send(True, log_data, log_plt, ind)

            vis = self.annotation.get_visible()
            if vis and mouse_event_not_in_any_sub_plot:
                self.annotation.set_visible(False)
            self.fig.canvas.draw_idle()


    def draw_line_from_mouse_that_pressed_on_client_or_server_receive_to_the_corresponding_send(self, doing_server_receive: bool, client_or_server_receive_logs, log_plt, ind):

        point_index_of_closest_point_in_line_to_mouse = ind['ind'][0]
        closest_idx = point_index_of_closest_point_in_line_to_mouse
        log_message = client_or_server_receive_logs[closest_idx][1]

        x, y = x_y_data_from_plot_or_scatter(log_plt)
        client_receive_point = (x[closest_idx], y[closest_idx])
        client_cihtems = extract_cihtems_from_log_message(log_message)

        logs_to_search_through = None
        if doing_server_receive:
            logs_to_search_through = log_type_to_logs[LogType.client.sending_input_snapshot]
        else: # doing client receive
            logs_to_search_through = log_type_to_logs[LogType.server.sending_game_state]


        self.draw_line_between_matching_cihtems(doing_server_receive, client_receive_point, client_cihtems, logs_to_search_through)


    # def draw_input_snapshot_server_send_to_client_receive_game_state_line(self, client_receive_game_state_logs, log_plt, ind):
    #     point_index_of_closest_point_in_line_to_mouse = ind['ind'][0]
    #     closest_idx = point_index_of_closest_point_in_line_to_mouse
    #     log_message = client_receive_game_state_logs[closest_idx][1]
    #
    #     x, y = x_y_data_from_plot_or_scatter(log_plt)
    #     client_receive_point = (x[closest_idx], y[closest_idx])
    #     client_cihtems = extract_cihtems_from_log_message(log_message)
    #     logs_to_search_through = log_type_to_logs[LogType.server.sending_game_state]
    #
    #     self.draw_line_between_matching_cihtems(client_receive_point, client_cihtems, logs_to_search_through)


    def draw_line_between_matching_cihtems(self, doing_server_receive: bool,  origin_cihtems_point, cihtems_to_find, logs_to_search_through):
        cihtems_found = False
        send_point = (0, 0)
        for point, log_message in logs_to_search_through:
            server_cihtems = extract_cihtems_from_log_message(log_message)            
            if cihtems_to_find == server_cihtems:
                cihtems_found = True
                # need to convert here because raw data is a datetime, don't need to do elsewhere
                # because once on graph it's already converted
                send_point = (mdates.date2num(point[0]), point[1])

        if cihtems_found:
            print(origin_cihtems_point, send_point)
            self.annotation_lines.add_line(origin_cihtems_point[0], origin_cihtems_point[1], send_point[0], send_point[1], color= ('black' if doing_server_receive else 'grey'))
        

    def draw_input_snapshot_client_to_server_processed_line(self, client_physics_tick_logs, log_plt, ind):
        """not may not draw line if server doesn't ever process the given input"""

        point_index_of_closest_point_in_line_to_mouse = ind['ind'][0]
        closest_idx = point_index_of_closest_point_in_line_to_mouse
        log_message = client_physics_tick_logs[closest_idx][1]

        x, y = x_y_data_from_plot_or_scatter(log_plt)
        client_process_point = (x[closest_idx], y[closest_idx])

        client_cihtems = extract_cihtems_from_log_message(log_message)

        client_input_snapshot_was_processed_by_server = False
        receive_point = (0, 0)
        for point, log_message in log_type_to_logs[LogType.server.updated_player_state_with_input_snapshot]:
            server_cihtems = extract_cihtems_from_log_message(log_message)
            if client_cihtems == server_cihtems:
                client_input_snapshot_was_processed_by_server = True
                # need to convert here because raw data is a datetime, don't need to do elsewhere
                # because once on graph it's already converted
                receive_point = (mdates.date2num(point[0]), point[1])


        if client_input_snapshot_was_processed_by_server:
            print(client_process_point, receive_point)
            self.annotation_lines.add_line(client_process_point[0], client_process_point[1], receive_point[0], receive_point[1])



    def handle_drawing_of_vertical_lines(self, event, log_plt, ind):

        point_index_of_closest_point_in_line_to_mouse = ind['ind'][0]
        closest_idx = point_index_of_closest_point_in_line_to_mouse

        x, _ = x_y_data_from_plot_or_scatter(log_plt)

        while self.vlines:
            vline = self.vlines.pop()
            vline.remove()
        # Draw a new vertical line at the x position of the mouse
        vline = self.ax.axvline(x[closest_idx], color='gray', linestyle='--')
        self.vlines.append(vline)
        # Update the plot to show the new line

    # Function to update annotation
    def update_annot(self, log_plt, annot, ind, messages, pinned_annotation_requested: bool) -> None:
        point_index_of_closest_point_in_line_to_mouse = ind['ind'][0]
        closest_idx = point_index_of_closest_point_in_line_to_mouse

        x, y = x_y_data_from_plot_or_scatter(log_plt)

        if pinned_annotation_requested:

            already_have_annotation_at_this_point =  f"{x}{y}" in self.xy_to_pinned_annotation
            if not already_have_annotation_at_this_point: # you have to clear the annotation to do this
                annot = self.ax.annotate("", xy=(0,0), xytext=(-20,20), textcoords="offset points",
                                bbox=dict(boxstyle="round", fc="w", alpha=0.4),
                                arrowprops=dict(arrowstyle="->"))
                self.xy_to_pinned_annotation[f"{x}{y}"] = annot


        annot.xy = (x[closest_idx], y[closest_idx]) # move the annotation
        # the following line works because the the index of a point is the same as the index of its message
        # text = f'Timestamp: {x[closest_idx]}\nMessage: {messages[closest_idx]}' # set the text
        text = f'Message: {messages[closest_idx]}' # set the text
        annot.set_text(text)
        annot.set_visible(True)


# everything happens below.

def x_y_data_from_plot_or_scatter(plot_or_scatter):
    if isinstance(plot_or_scatter, PathCollection): #scatter plot
        x, y = plot_or_scatter.get_offsets().T
    elif isinstance(plot_or_scatter, Line2D): # regular plot
        x, y = plot_or_scatter.get_data()
    return (x, y)

def plot_data_onto_plots(log_type_to_logs, fig, ax):
    input_snapshot_receive_status = create_client_to_server_input_snapshot_sent_status(log_type_to_logs[LogType.client.physics_tick], log_type_to_logs[LogType.server.updated_player_state_with_input_snapshot])

    for log_type in log_type_to_logs.keys():

        if log_type == LogType.client.physics_tick:
            color = [('green' if recv else 'red') for recv in input_snapshot_receive_status]
        else:
            color = 'r'

        logs = log_type_to_logs[log_type]
        log_positions, log_messages = zip(*logs)
        # log_type_to_plt[log_type] = ax.plot(*zip(*log_positions), 'o', color=color, label=log_type.name)[0]
        log_type_to_plt[log_type] = ax.scatter(*zip(*log_positions), color=color, label=log_type.name)

    # Format x-axis to show timestamps nicely
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S.%f'))
    ax.tick_params(axis='x', rotation=45, labelsize=8)  # Increase x tick label size
    # ax.xaxis.set_major_locator(mdates.MicrosecondLocator(interval=10**5))  # Set tick frequency to every 100 milliseconds

    # Show faint gray grid lines for each x tick
    ax.grid(axis='x', color='gray', linestyle='--', alpha=0.5)

    # Show faint gray grid lines for each y tick
    ax.grid(axis='y', color='gray', linestyle='--', alpha=0.5)

    # Hide y-axis ticks
    ax.yaxis.set_ticks([])


    annotation = ax.annotate("", xy=(0,0), xytext=(-20,20), textcoords="offset points",
                        bbox=dict(boxstyle="round", fc="w", alpha=0.4),
                        arrowprops=dict(arrowstyle="->"))
    annotation.set_visible(False)

    # Add y-axis labels
    y_tick_positions = []
    y_tick_labels = []
    for log_type, y_position in log_type_to_y_position.items():
        y_tick_positions.append(y_position)
        y_tick_labels.append(log_type.name)

    ax.set_yticks(y_tick_positions)
    ax.set_yticklabels(y_tick_labels)

    # Add title
    ax.set_title('client Server Reconciliation', fontsize=14)
    global hover_tracker, scroll_tracker

    hover_tracker = GraphInteraction(ax, log_type_to_logs, annotation, fig)
    scroll_tracker = ScrollTracker(fig, ax)

    assert hover_tracker is not None and scroll_tracker is not None 
    fig.canvas.mpl_connect("motion_notify_event", lambda e: hover_tracker.hover(e))
    fig.canvas.mpl_connect('button_press_event', lambda e: hover_tracker.set_mouse_pressed(e, True))
    fig.canvas.mpl_connect('button_release_event', lambda e: hover_tracker.set_mouse_pressed(e, False))
    fig.canvas.mpl_connect('key_press_event', lambda e: hover_tracker.on_key_press(e))
    fig.canvas.mpl_connect('key_release_event', lambda e: hover_tracker.on_key_release(e))
    # fig.canvas.mpl_connect('scroll_event', lambda e: scroll_tracker.on_scroll(e))

def clamp(v, m, M):
    return min(max(m, v), M)

class ScrollTracker():
    def __init__(self, fig, ax):
        self.fig = fig
        self.ax = ax


    def on_scroll(self, event):
        increment = 1 if event.button == 'up' else -1
        if increment == 1:
            print("got up")
        else:
            print("got down")
        # global start_line, end_line

        # start_line = clamp(start_line + increment * lines_per_screen // 3, 0, client_total_lines - 1)
        # end_line = clamp(start_line + lines_per_screen, 0, client_total_lines - 1) # don't take last line, might be invalid due to ctrl-c

        # self.reload_graph_for_new_line_range()

    # def reload_graph_for_new_line_range(self):
    #     self.ax.clear()
    #     # Read log data from file within specified range
    #     log_data = entire_client_log_data[start_line:end_line]
    #     log_type_to_logs = parse_log_data_to_dict_of_log_type_to_tuples_of_time_to_message(log_data)
    #
    #     assert hover_tracker is not None 
    #     hover_tracker.log_type_to_logs = log_type_to_logs
    #     plot_data_onto_plots(log_type_to_logs, self.fig, self.ax);
    #     self.fig.canvas.draw_idle()


def extract_cihtems_from_log_message(log_message: str) -> int:

    # Define a regex pattern to extract the number after client_history_insertion_time_epoch_ms
    pattern = r'Client Input History Insertion Time \(epoch ms\): (\d+)'

    # Search for the pattern in the log string
    match = re.search(pattern, log_message)

    if match:
        client_history_insertion_time_epoch_ms = match.group(1)
        return int(client_history_insertion_time_epoch_ms)
    else:
        return -1 

def create_client_to_server_input_snapshot_sent_status(client_physics_tick_logs, server_updated_player_state_with_input_snapshot_logs) -> List[bool]:
    """creates a boolean array for each client physics tick which is true iff the input snapshot used to 
        during the client physics tick is ever received by the server"""

    client_cithems_processed = [extract_cihtems_from_log_message(log[1]) for log in client_physics_tick_logs]
    server_cithems_processed = {extract_cihtems_from_log_message(log[1]) for log in server_updated_player_state_with_input_snapshot_logs}


    processed_array = []
    for client_cithems in client_cithems_processed:
        print(client_cithems in server_cithems_processed)
        processed_array.append(client_cithems in server_cithems_processed)

    return processed_array

def process_log_file(input_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    single_line_logs = []
    current_log = ''
    
    for line in lines:
        # Check if the line starts with a timestamp which indicates the beginning of a new log entry
        match = re.match(r'(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{9}\] \[\w+\])', line)
        if match:
            # If there's an existing log entry being processed, save it
            if current_log:
                single_line_logs.append(current_log)
            # Start a new log entry and add a line break after the timestamp and log type
            current_log = match.group(1) + '\n' + line[match.end():]
        else:
            # Continue the current log entry
            current_log += line
    
    # Don't forget to add the last log entry if it exists
    if current_log:
        single_line_logs.append(current_log)
    
    return single_line_logs

hover_tracker, scroll_tracker = None, None

client_log_file_path = '../client/build/logs.txt'

if use_remote_server_logs:
    server_log_file_path = '../server/build/remote_logs.txt'
else:
    server_log_file_path = '../server/build/logs.txt'

entire_client_log_data = process_log_file(client_log_file_path)
entire_server_log_data = process_log_file(server_log_file_path)

# with open(client_log_file_path) as file:
#     entire_client_log_data = file.readlines()
#
# with open(server_log_file_path) as file:
#     entire_server_log_data = file.readlines()

client_total_lines = len(entire_client_log_data)
server_total_lines = len(entire_server_log_data)

# globals
client_lines_per_screen = client_total_lines - 2 # last few lines may be cut off
client_start_line = 0
client_end_line = client_start_line + client_lines_per_screen

server_lines_per_screen = server_total_lines - 2 # last few lines may be cut off
server_start_line = 0
server_end_line = server_start_line + server_lines_per_screen


# Read log data from file within specified range
client_log_data = entire_client_log_data[client_start_line:client_end_line]
server_log_data = entire_server_log_data[server_start_line:server_end_line]
log_type_to_logs = parse_log_data_to_dict_of_log_type_to_tuples_of_time_to_message(client_log_data, server_log_data)

def do_everything():


    # Create scatter plot with annotations for each type of log
    fig, ax = plt.subplots(figsize=(12, 6))
    plot_data_onto_plots(log_type_to_logs, fig, ax);
    plt.show()

do_everything()
