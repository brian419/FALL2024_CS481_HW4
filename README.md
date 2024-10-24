# CS481 HW4: Game of Life MPI Implementation

## Jeongbin Son  
CWID: 12067329

## Project Description
This project implements an MPI-based version of Conway's Game of Life for CS481 High Performance Computing. It uses message passing with MPI to distribute the game board across multiple processes.

## How to Compile
To compile the program, use the following command:
```bash
mpicc -o game_of_life_mpi src/game_of_life_mpi.cpp



### Personal note for me for now:
mpirun -np <num_processes> ./game_of_life_mpi <board_size> <max_iterations> <output_directory>
