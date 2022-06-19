# CIDA - Control Infrastructure for Distributed Algorithms

CIDA has the ambition to aid the learning and understanding a distributed algorithm's inner workings.

CIDA is a two-tier architecture, with the basic layer sitting on top of the control layer.

The idea behind having two layers is to allow the user of the interface to use the basic layer to implement its algorithm while the control layer works behind the scenes and has a probabilistic or clock signaled algorithm to inform the user of the algorithm's evolution, such as deadlock detection or termination detection.


## Examples

You can find two defined examples.

 - [Point-To-Point Communication](/documentation/examples.md)

 - [Election Algorithm](/documentation/election.md)

## Documentation

In the documentation you can find some decisions behind the infrastructure's design, and
the skeletons of the implemented functions. And the topology's specification.
 - [Documentation](/documentation/cida.md)
 - [Topology Specification](/documentation/topology_specification)
