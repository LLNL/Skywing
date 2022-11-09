import matplotlib.pyplot as plt
import sys

# Script to plot results of the collective count example.
# First argument is name of output file.
# Second argument (optional) is name of file to which to save plot.

filename = sys.argv[1]
res_dict = {}
with open(filename, 'r') as f:
    for line in f:
        tokens = line.split()
        time = int(tokens[0][:-3])
        machine_ind = int(tokens[2])
        count = int(tokens[5].rstrip(','))
        if machine_ind not in res_dict:
            res_dict[machine_ind] = [[],[]]
        res_dict[machine_ind][0].append(time)
        res_dict[machine_ind][1].append(count)


for ind, results in res_dict.items():
    plt.plot(results[0], results[1])

plt.ylim([0, 80])
plt.xlabel("Time (ms)")
plt.ylabel("Count")

if len(sys.argv) > 2:
    plt.savefig(sys.argv[2])
else:
    plt.show()
