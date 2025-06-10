import csv
import matplotlib.pyplot as plt

runNumber = 5 # Choose the number of the run to plot

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
