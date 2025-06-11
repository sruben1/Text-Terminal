import csv
import matplotlib.pyplot as plt

runNumber = 1 # Choose the number of the run to plot

# Plot 1: Different inserts and consequences for gui
x = range(1, 1001, 10)
with open(f'profilerRuns/run{runNumber}/0-Run-mediumFile.csv', newline='') as csvfile:
    reader = list(csv.reader(csvfile))
    insert_opt = [float(row[1]) for row in reader[25::20]]
    gui_opt = [float(row[1]) for row in reader[26::20]]
with open(f'profilerRuns/run{runNumber}/1-Run-mediumFile.csv', newline='') as csvfile:
    reader = list(csv.reader(csvfile))
    insert_nonopt = [float(row[1]) for row in reader[25::20]]
    gui_nonopt = [float(row[1]) for row in reader[26::20]]
with open(f'profilerRuns/run{runNumber}/2-Run-mediumFile.csv', newline='') as csvfile:
    reader = list(csv.reader(csvfile))
    insert_mixed = [float(row[1]) for row in reader[25::20]]
    gui_mixed = [float(row[1]) for row in reader[26::20]]
    
# plt.plot(x, insert_opt, marker='')
plt.plot(x, gui_opt, marker='', label='Optimized')
# plt.plot(x, insert_nonopt, marker='', )
plt.plot(x, gui_nonopt, marker='', label='Non-Optimized')
# plt.plot(x, insert_mixed, marker='')
plt.plot(x, gui_mixed, marker='', label='Mixed')
plt.xlabel('Number of inserts')
plt.ylabel('Runtime in seconds')
plt.title('GUI Rendering Times with Different Inserts')
plt.legend()
plt.tight_layout()
plt.savefig(f'profilerRuns/run{runNumber}/plot1.png')


# Plot 4 showing insert times for all file sizes on log scale for subtle differences
x = range(1, 1001, 10)

# Read insert data for all file sizes
file_sizes = ['smallFile', 'mediumFile', 'veryBigFile']
insert_data = {}

for file_size in file_sizes:
    with open(f'profilerRuns/run{runNumber}/0-Run-{file_size}.csv', newline='') as csvfile:
        reader = list(csv.reader(csvfile))
        insert_opt = [float(row[1]) for row in reader[25::20]]
    with open(f'profilerRuns/run{runNumber}/1-Run-{file_size}.csv', newline='') as csvfile:
        reader = list(csv.reader(csvfile))
        insert_nonopt = [float(row[1]) for row in reader[25::20]]
    with open(f'profilerRuns/run{runNumber}/2-Run-{file_size}.csv', newline='') as csvfile:
        reader = list(csv.reader(csvfile))
        insert_mixed = [float(row[1]) for row in reader[25::20]]
    
    insert_data[file_size] = {
        'opt': insert_opt,
        'nonopt': insert_nonopt,
        'mixed': insert_mixed
    }

plt.figure()
for file_size in file_sizes:
    # Calculate average across the three optimization types
    insert_avg = []
    for i in range(len(x)):
        avg_value = (insert_data[file_size]['opt'][i] + insert_data[file_size]['nonopt'][i] + insert_data[file_size]['mixed'][i]) / 3
        insert_avg.append(avg_value)
    
    plt.plot(x, insert_avg, marker='', label=f'{file_size}')

plt.xlabel('Number of inserts')
plt.ylabel('Runtime in seconds (log)')
plt.title('Insert Times Across File Sizes')
plt.yscale('log') 
plt.legend()
plt.tight_layout()
plt.savefig(f'profilerRuns/run{runNumber}/plot4-insert_times.png')


# Plot 5 showing insert time slowdown comparisons (averaged across optimization types)
plt.figure()

# Calculate averages across optimization types
medium_vs_small_avg = []
large_vs_small_avg = []
large_vs_medium_avg = []

for i in range(len(x)):
    # Medium vs Small average
    mvs_values = [insert_data['mediumFile'][opt_type][i]/insert_data['smallFile'][opt_type][i] for opt_type in ['opt', 'nonopt', 'mixed']]
    medium_vs_small_avg.append(sum(mvs_values) / len(mvs_values))
    
    # Large vs Small average
    lvs_values = [insert_data['veryBigFile'][opt_type][i]/insert_data['smallFile'][opt_type][i] for opt_type in ['opt', 'nonopt', 'mixed']]
    large_vs_small_avg.append(sum(lvs_values) / len(lvs_values))
    
    # Large vs Medium average
    lvm_values = [insert_data['veryBigFile'][opt_type][i]/insert_data['mediumFile'][opt_type][i] for opt_type in ['opt', 'nonopt', 'mixed']]
    large_vs_medium_avg.append(sum(lvm_values) / len(lvm_values))

plt.plot(x, medium_vs_small_avg, marker='', label='Medium vs Small')
plt.plot(x, large_vs_small_avg, marker='', label='Large vs Small')
plt.plot(x, large_vs_medium_avg, marker='', label='Large vs Medium')

# Add average markers as dotted horizontal lines
mvs_overall_avg = sum(medium_vs_small_avg) / len(medium_vs_small_avg)
lvs_overall_avg = sum(large_vs_small_avg) / len(large_vs_small_avg)
lvm_overall_avg = sum(large_vs_medium_avg) / len(large_vs_medium_avg)

plt.axhline(y=mvs_overall_avg, color='C0', linestyle=':', alpha=0.7, label=f'Medium vs Small Avg: {mvs_overall_avg:.2f}')
plt.axhline(y=lvs_overall_avg, color='C1', linestyle=':', alpha=0.7, label=f'Large vs Small Avg: {lvs_overall_avg:.2f}')
plt.axhline(y=lvm_overall_avg, color='C2', linestyle=':', alpha=0.7, label=f'Large vs Medium Avg: {lvm_overall_avg:.2f}')

plt.xlabel('Number of inserts')
plt.ylabel('Slowdown factor')
plt.title('Insert Time Slowdown Comparisons')
plt.legend()
plt.tight_layout()
plt.savefig(f'profilerRuns/run{runNumber}/plot5-insert_slowdown.png')

# Plot 6 showing render times for all file sizes on standard scale
gui_data = {}

for file_size in file_sizes:
    with open(f'profilerRuns/run{runNumber}/0-Run-{file_size}.csv', newline='') as csvfile:
        reader = list(csv.reader(csvfile))
        gui_opt = [float(row[1]) for row in reader[26::20]]
    with open(f'profilerRuns/run{runNumber}/1-Run-{file_size}.csv', newline='') as csvfile:
        reader = list(csv.reader(csvfile))
        gui_nonopt = [float(row[1]) for row in reader[26::20]]
    with open(f'profilerRuns/run{runNumber}/2-Run-{file_size}.csv', newline='') as csvfile:
        reader = list(csv.reader(csvfile))
        gui_mixed = [float(row[1]) for row in reader[26::20]]
    
    gui_data[file_size] = {
        'opt': gui_opt,
        'nonopt': gui_nonopt,
        'mixed': gui_mixed
    }

plt.figure()
for file_size in file_sizes:
    plt.plot(x, gui_data[file_size]['opt'], marker='', label=f'{file_size} - Optimized')
    plt.plot(x, gui_data[file_size]['nonopt'], marker='', label=f'{file_size} - Non-Optimized')
    plt.plot(x, gui_data[file_size]['mixed'], marker='', label=f'{file_size} - Mixed')

plt.xlabel('Number of inserts')
plt.ylabel('Runtime in seconds')
plt.title('Render Times Across File Sizes')
plt.legend()
plt.tight_layout()
plt.savefig(f'profilerRuns/run{runNumber}/plot6-render_times.png')

# Plot 2: Undo / Redo times with growing node amount
x = range(1000, 100000, 1000)
with open(f'profilerRuns/run{runNumber}/3-Run-veryBigFile.csv', newline='') as csvfile:
    reader = list(csv.reader(csvfile))
    node_search = [float(row[1]) for row in reader[23::4]]
    undo = [float(row[1]) for row in reader[25::4]]
    redo = [float(row[1]) for row in reader[26::4]]

plt.figure()
plt.plot(x, node_search, marker='', label='Node Search')
plt.plot(x, undo, marker='', label='Undo')
plt.plot(x, redo, marker='', label='Redo', linestyle='--')
plt.xlabel('Node amount')
plt.ylabel('Runtime in seconds')
plt.title('Undo / Redo Times with Growing Node Amount')
plt.legend()
plt.tight_layout()
plt.savefig(f'profilerRuns/run{runNumber}/plot2.png')

# Plot 3: Search times with and without cache
x = range(0, 100000, 1000)
with open(f'profilerRuns/run{runNumber}/4-Run-veryBigFile.csv', newline='') as csvfile:
    reader = list(csv.reader(csvfile))
    search_no_cache = [float(row[1]) for row in reader[23:]]
with open(f'profilerRuns/run{runNumber}/5-Run-veryBigFile.csv', newline='') as csvfile:
    reader = list(csv.reader(csvfile))
    search_cache = [float(row[1]) for row in reader[23:]]

plt.figure()
plt.plot(x, search_no_cache, marker='', label='Search without cache')
plt.plot(x, search_cache, marker='', label='Search with cache')
plt.xlabel('Position of search result in text')
plt.ylabel('Runtime in seconds')
plt.title('Effect of Line Number Caching on Search Times')
plt.legend()
plt.tight_layout()
plt.savefig(f'profilerRuns/run{runNumber}/plot3.png')
