#!/bin/bash
#PBS -N game_of_life_mpi_job
#PBS -l nodes=1:ppn=1
#PBS -l walltime=10:00:00
#PBS -l mem=4gb
#PBS -q class

cd $PBS_O_WORKDIR

module load openmpi
mpicc -o game_of_life_mpi src/game_of_life_mpi.cpp -lstdc++ -std=c++11
mpirun -np 1 ./game_of_life_mpi 5000 5000 /scratch/ualclsd0197/output_dir | tee /scratch/ualclsd0197/gameoflife_output.txt

