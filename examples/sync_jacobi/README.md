```
           _|                                        _|
   _|_|_|  _|  _|    _|    _|  _|_|_|      _|_|    _|_|_|_|
 _|_|      _|_|      _|    _|  _|    _|  _|_|_|_|    _|
     _|_|  _|  _|    _|    _|  _|    _|  _|          _|
 _|_|_|    _|    _|    _|_|_|  _|    _|    _|_|_|      _|_|
                           _|
                       _|_|
```

## (Synchronous) Jacobi Method 

## Summary 
This is example showcases the Jacobi Method implemented in the Skywing upper for solving a system of linear equations, name, solving for x in Ax=b. 
This method requires that the underlying system be square and strictly diagonally dominant, hence there exists a unique solution.
Unlike most implementations, this implementation is decentralized, so this does not follow the manager-slave model commonly found in parallel computing, so the "indexing" of the components passed between Skywing agents has to be explicit rather than some implicit partitioning scheme.
The user is required to pass in a row partition matrix A and vector b, as well as the row indices that these rows correspond to, starting to index from 0.
This also means that this method is agnostic to the partitioning of the data, meaning that non-uniform partitioning is not a problem and even expected.
While not necessarily useful since this is a synchronous operation, this implementation is also agnostic to overlapping computations, so if a user partitions the same row to two Skywing agents, each Skywing agent ignores the specific component that is overlapping by keeping it's own update, and every other Skywing agent simply chooses the update it has stored most recently for computing it's next update.
This method needs a minimum of 2 Skywing agents to work properly, and this example needs separate files for the A partition, b partition, and row indices to run properly, as well as solution vector x, since this is an example with terminal outputs, for EACH Skywing agent.
The naming convention used in the example for data input for each Skywing agent can be obtained by observing sync_jacobi.cpp, and the accompanying python preprocessing script named jacobi_pre_processing.py.
This implementation assumes that all files are in matrix market format, which is what jacobi_pre_processing.py outputs in a partition directory.


## Brief 
#### Inputs: 
- Row partition of b 
- Row partition of A 
- Row indices of corresponding to the row partition of A and b 

#### Output: 
- Solution vector x at each Skywing Agent

#### Run Example
- To run the example, merely use ./run.sh [starting port number] in the build directory. 