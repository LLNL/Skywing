import os
import numpy as np
import math
from pathlib import Path
from scipy.io import mmread
from scipy.io import mmwrite
import sys


def print_linear_system (A,x,b):
    [m,n] = np.shape(A)
    for i in range(m):
        if i != np.floor(m/2):
            print(A[i,:], x[i], "   ", b[i])
        else:
            print(A[i,:], x[i], " = ", b[i])

def system_check(A,x_sol,b):
    # This is a check if the linear system has been generated correctly.
    Ax = np.matmul(A,x_sol)
    residual = np.linalg.norm(Ax - b)
    if (residual > 10-6):
        print("System is NOT accurate  -> ||Ax - b|| = " , residual)
        if np.prod(np.shape(b)) < 10:
            print("Expected b:")
            b_expected = np.matmul(A,x_sol)
            print(b_expected)
            x_sol_new= np.linalg.lstsq(A, b, rcond=None)[0]
            print("Expected x:")
            print(x_sol_new)
            error_from_input = np.linalg.norm(x_sol_new - x_sol)
            residual_from_input = np.linalg.norm(b-b_expected)
            print("||x_input - x_least_squares||: " , error_from_input)
            print("||Ax - b||: " , residual_from_input)
            filename = system_hold_folder + "/" + "x_sol_new_" +  name_of_system + ".mtx"
            mmwrite(filename, x_sol_new)
            filename = system_hold_folder + "/" + "b_expected_" +  name_of_system + ".mtx"
            mmwrite(filename, b_expected)
    else:
        print("System is accurate  -> ||Ax - b|| = " , residual)


dir = Path('.')
n = len(sys.argv)

starting_redundancy = int(sys.argv[1])
ending_redundancy = int(sys.argv[2])
size_of_network = int(sys.argv[3])
name_of_system = sys.argv[4]
system_hold_folder = sys.argv[5]
partition_hold_folder = sys.argv[6]


A_filename = system_hold_folder + "/" + name_of_system
b_filename = system_hold_folder + "/" + "rhs_" +  name_of_system
x_sol_filename = system_hold_folder + "/" + "x_sol_" +  name_of_system

A=mmread(A_filename)
b = mmread(b_filename)
x_sol = mmread(x_sol_filename)

[matrix_size,_] = np.shape(A)
row_count = np.zeros(size_of_network, dtype=int)
for i in range(matrix_size):
    row_count[i%size_of_network] +=1

for redundancy in range(starting_redundancy, ending_redundancy+1):
    rank_starting_index = 0
    for rank in range(0,size_of_network):
        row_hold = A[rank_starting_index,:]
        for row in range(1, redundancy + row_count[rank]):
            if((rank_starting_index + row) < matrix_size):
                row_hold = np.vstack((row_hold,A[(rank_starting_index + row),:]))
            else:
                row_hold = np.vstack((row_hold,A[(rank_starting_index + row)%matrix_size,:]))

        if (redundancy + row_count[rank]) == 1:
            row_hold=row_hold[None,:]

        A_partition_filename =  partition_hold_folder + "/machine_" + str(rank) + "_row_count_" + str(redundancy) + "_" + name_of_system
        mmwrite(A_partition_filename, row_hold)
        rank_starting_index += row_count[rank]

    rank_starting_index = 0
    for rank in range(0,size_of_network):
        row_hold=b[rank_starting_index]
        for row in range(1,redundancy+ row_count[rank]):
            if((rank_starting_index + row) < matrix_size):
                row_hold=np.vstack((row_hold,b[(rank_starting_index + row)]))
            else:
                row_hold=np.vstack((row_hold,b[(rank_starting_index + row) % matrix_size]))
        if redundancy + row_count[rank]==1:
            row_hold=row_hold[:,None]
        b_partition_filename =  partition_hold_folder + "/machine_" + str(rank) + "_row_count_" + str(redundancy) + "_rhs_" + name_of_system
        mmwrite(b_partition_filename, row_hold)
        rank_starting_index += row_count[rank]

    rank_starting_index = 0
    for rank in range(0, size_of_network):
        row_hold=x_sol[rank_starting_index]
        for row in range(1,redundancy + row_count[rank]):
            if((rank_starting_index + row) < matrix_size):
                row_hold=np.vstack((row_hold,x_sol[(rank_starting_index + row)]))
            else:
                row_hold=np.vstack((row_hold,x_sol[(rank_starting_index + row) % matrix_size]))
        if redundancy + row_count[rank] == 1:
            row_hold=row_hold[:,None]
        x_sol_partition_filename =  partition_hold_folder + "/machine_" + str(rank) + "_row_count_" + str(redundancy) + "_x_sol_" + name_of_system
        mmwrite(x_sol_partition_filename, row_hold)
        rank_starting_index += row_count[rank]


    rank_starting_index = 0
    for rank in range(0,size_of_network):
        index_hold = np.array([rank_starting_index])
        for row in range(1,(redundancy + row_count[rank])):
            if((rank_starting_index + row)<matrix_size):
                index_hold = np.append(index_hold, rank_starting_index + row)
            else:
                index_hold = np.append(index_hold, ((rank_starting_index + row)%matrix_size))
        index_hold = index_hold[None,:]
        index_hold_filename =  partition_hold_folder + "/machine_" + str(rank) + "_row_count_" + str(redundancy) + "_indices_" + name_of_system
        mmwrite(index_hold_filename, index_hold)
        rank_starting_index += row_count[rank]

# Checks if the linear system is correct.
system_check(A,x_sol,b)
