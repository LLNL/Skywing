import matplotlib.pyplot as plt
import numpy as np
import sys

# Script to plot results of the collective count example.
# First argument is name of output file.
# Second argument is correct value
# Third argument (optional) is name of file to which to save plot.

filename = sys.argv[1]
correct_val = float(sys.argv[2])
res_dict = {}
with open(filename, 'r') as f:
    for line in f:
        tokens = line.split()
        if tokens[0] == "Starting":
            continue
        time = int(tokens[0][:-3])
        machine_ind = int(tokens[2])
        value = float(tokens[-1])
        if machine_ind not in res_dict:
            res_dict[machine_ind] = [[],[]]
        res_dict[machine_ind][0].append(time)
        res_dict[machine_ind][1].append(value - correct_val)

for ind, results in res_dict.items():
    plt.semilogy(results[0], np.abs(results[1]))

#plt.ylim([-400, 400])
plt.xlabel("Time (ms)")
plt.ylabel("Error")

if len(sys.argv) > 3:
    plt.savefig(sys.argv[3])
else:
    plt.show()
