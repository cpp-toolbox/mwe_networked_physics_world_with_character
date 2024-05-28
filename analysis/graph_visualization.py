import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime

# Function to parse log data and sort into respective arrays
def parse_log_data_to_lists_of_tuples_of_time_to_label(log_data):
    unique_id_logs = []
    game_update_logs = []
    player_update_logs = []
    physics_tick_logs = []
    for log in log_data:
        timestamp_str = log.split(']')[0][1:-3]  # Remove nanoseconds
        timestamp = datetime.strptime(timestamp_str, '%Y-%m-%d %H:%M:%S.%f')
        message = log.strip()  # Remove leading/trailing whitespaces
        if "Received unique ID from server" in message:
            unique_id_logs.append((timestamp, message))
        elif "Just received a game update" in message:
            game_update_logs.append((timestamp, message))
        elif "just got the jolt lock to update player" in message:
            player_update_logs.append((timestamp, message))
        elif "physics tick with delta" in message:
            physics_tick_logs.append((timestamp, message))
    return unique_id_logs, game_update_logs, player_update_logs, physics_tick_logs

def hover(event):
    if event.inaxes == ax:
        # for line, annot, messages in zip(lines, annots, [unique_id_messages, game_update_messages, player_update_messages, physics_tick_messages]):
        all_messages = [unique_id_messages, game_update_messages, player_update_messages, physics_tick_messages]

        for line, annot, messages in zip(lines, annots,all_messages ):
            # print(len(line.get_data()[0]), len(messages))
            cont, ind = line.contains(event)
            # A dictionary {'ind': pointlist}, where pointlist is a list of points of the line that are within the pickradius around the event position.
            if cont:
                update_annot(line, annot, ind, messages)
                annot.set_visible(True)
                fig.canvas.draw_idle()
            else:
                vis = annot.get_visible()
                if vis:
                    annot.set_visible(False)
                    fig.canvas.draw_idle()

# Function to update annotation
def update_annot(line, annot, ind, messages):
    point_index_of_closest_point_in_line_to_mouse = ind['ind'][0]
    closest_idx = point_index_of_closest_point_in_line_to_mouse
    x, y = line.get_data()
    annot.xy = (x[closest_idx], y[closest_idx])
    #print(len(line), len(messages), closest_idx)
    text = f'Timestamp: {x[closest_idx]}\nMessage: {messages[closest_idx]}'
    annot.set_text(text)

# Read log data from file within specified range
log_file_path = '../client/build/logs.txt'
start_line = 1000  # Specify start line
end_line = start_line + 1000  # Specify end line

with open(log_file_path) as file:
    log_data = file.readlines()[start_line:end_line]

# Parse log data and sort into respective arrays
unique_id_logs, game_update_logs, player_update_logs, physics_tick_logs = parse_log_data_to_lists_of_tuples_of_time_to_label(log_data)

# Convert timestamps to numbers for plotting
# zip turns them into a list of their times, and a list of their messages
unique_id_timestamps, unique_id_messages = zip(*unique_id_logs) if unique_id_logs else ([], [])
game_update_timestamps, game_update_messages = zip(*game_update_logs)
player_update_timestamps, player_update_messages = zip(*player_update_logs)
physics_tick_timestamps, physics_tick_messages = zip(*physics_tick_logs)


# Create scatter plot with annotations for each type of log
fig, ax = plt.subplots(figsize=(12, 6))

start_y = 1
gap_size = 0.25

# Create scatter plot with annotations
lines = []
labels = ['Unique ID Logs', 'Game Update Logs', 'Player Update Logs', 'Physics Tick Logs']

line1, = ax.plot(unique_id_timestamps, [start_y]*len(unique_id_logs), 'o', color='r', label=labels[0])
lines.append(line1)

if game_update_logs:
    line2, = ax.plot(game_update_timestamps, [start_y + gap_size]*len(game_update_logs), 'o', color='g', label=labels[1])
    lines.append(line2)
if player_update_logs:
    line3, = ax.plot(player_update_timestamps, [start_y + 2 * gap_size]*len(player_update_logs), 'o', color='b', label=labels[2])
    lines.append(line3)
if physics_tick_logs:
    line4, = ax.plot(physics_tick_timestamps, [start_y + 3 * gap_size]*len(physics_tick_logs), 'o', color='m', label=labels[3])
    lines.append(line4)


# Format x-axis to show timestamps nicely
ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S.%f'))
ax.tick_params(axis='x', rotation=45, labelsize=8)  # Increase x tick label size
ax.xaxis.set_major_locator(mdates.MicrosecondLocator(interval=10**5))  # Set tick frequency to every 100 milliseconds

# Show faint gray grid lines for each x tick
ax.grid(axis='x', color='gray', linestyle='--', alpha=0.5)

# Show faint gray grid lines for each y tick
ax.grid(axis='y', color='gray', linestyle='--', alpha=0.5)

# Hide y-axis ticks
ax.yaxis.set_ticks([])

# Initialize annotations
annots = []
for _ in range(4):
    annot = ax.annotate("", xy=(0,0), xytext=(-20,20), textcoords="offset points",
                        bbox=dict(boxstyle="round", fc="w", alpha=0.4),
                        arrowprops=dict(arrowstyle="->"))
    annot.set_visible(False)
    annots.append(annot)


# Add y-axis labels
ax.set_yticks([start_y, start_y + gap_size, start_y + 2 * gap_size, start_y + 3 * gap_size])
ax.set_yticklabels(labels)

# Add title
ax.set_title('Client Server Reconciliation', fontsize=14)

# Connect hover event
fig.canvas.mpl_connect("motion_notify_event", hover)

plt.show()
