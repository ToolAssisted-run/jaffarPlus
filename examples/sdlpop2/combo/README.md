# Levels 1 and 2 in one run

`seed151.sol` plays a new game started with random seed 151: level 1 in 460 ticks (`0100/level01.sol`), then level 2
in 102 ticks (`0200/seeds/seed18.sol`). Level 3 starts at tick 562.

The seed was found by replaying the fastest level 1 and level 2 solutions one after the other under many seeds: with
seed 151 both hold, the guards of level 1 behave as in the solution, and level 2's sand tiles give the solution's trick.
