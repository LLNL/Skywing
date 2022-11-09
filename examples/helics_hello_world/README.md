# Helics Hello World Example

An example is provided where Skywing agents using [HELICS](https://helics.readthedocs.io/en/latest/) to pass messages.
HELICS matches subscriptions to publications by connected federates while keeping track of the simulation time associated with each publication or subscription request.
In this example, each Skywing agent generates a random number and then sends that number to every other Skywing agent through both Skywing and HELICS pub/sub mechanisms.
The received values are printed to the screen to verify they are the same.

## Things to note

 1. This example uses HELICS 2.x.
 1. The meson build system must be configured with the `-Dbuild_examples=true` and `-Duse_helics=true` flag in order for this example to be built.
 1. The executable must be ran with two arguments
   - the number of Skywing agents to include
   - the port number to start with for assigning listening ports for the agents
