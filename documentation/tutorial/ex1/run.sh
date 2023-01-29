#!/bin/bash

size_of_collective=2
starting_port=20000
for (( agent_ind = 0; agent_ind < $size_of_collective-1 ; agent_ind++ ))
do
    ./ex1 node$agent_ind $((starting_port+$agent_ind)) &
done
./ex1 node$agent_ind $((starting_port+$agent_ind))