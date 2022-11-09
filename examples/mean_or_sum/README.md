```
           _|                                        _|
   _|_|_|  _|  _|    _|    _|  _|_|_|      _|_|    _|_|_|_|
 _|_|      _|_|      _|    _|  _|    _|  _|_|_|_|    _|
     _|_|  _|  _|    _|    _|  _|    _|  _|          _|
 _|_|_|    _|    _|    _|_|_|  _|    _|    _|_|_|      _|_|
                           _|
                       _|_|
```

## Push-Sum 

## Summary 
This is example showcases a decentralized implementation of the asynchronous push-sum algorithm implemented in the Skywing upper found in "Full Asynchronous Push-sum with growing Intercommunication intervals for Olshevsky, et al. 
Push-sum is an asynchronous distributed averaging algorithm which takes a set of initial values as doubles, and outputs the average of the initial values.
The auxiliary variables are handled internally.
In this example, 4 agents store it's machine number + 1 as a double, hence the average is (1+2+3+4)/4 = 2.5, which is output at terminal.
The number of agents can be adjusted in the this example in run.sh.in by adjusting "size_of_network" variable, and the corresponding average is the converged value.
As a general caveat, if the communication involves message drops, the algorithm may converge to some other convex combination of initial values rather than the average of the initial values.  

## Brief 
#### Inputs: 
- Initial (real) value as a double 

#### Output: 
- Average of all initial values