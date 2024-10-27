/*
    Name: Jeongbin Son
    Email: json10@crimson.ua.edu
    Course Section: CS 481
    Homework #: 4
    To Compile: mpicc -o game_of_life_mpi src/game_of_life_mpi.cpp -lstdc++ -std=c++11
    To Run: mpirun -np <num_processes> ./game_of_life_mpi <board size> <max generations> <output directory>
    Note: I had to run conda install clangxx_osx-64 before i could run compilation
    mpirun -np 4 ./game_of_life_mpi 100 100 /path/to/output
    ex: mpirun -np 4 ./game_of_life_mpi 100 100 /Users/brianson/Desktop/cs481/hw4/FALL2024_CS481_HW4/output
    for the cluster: mpirun -np 4 ./game_of_life_mpi 5000 5000 /scratch/ualclsd0197/output_dir
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <mpi.h>

using namespace std;
using namespace std::chrono;

// this function will create an output directory
void creatingOutputDirectory(const string &outputDirectory) {
    struct stat directoryInfo;

    // if the directory doesn't exist, create it
    if (stat(outputDirectory.c_str(), &directoryInfo) != 0) {
        cout << "Creating directory: " << outputDirectory << endl;
        if (mkdir(outputDirectory.c_str(), 0777) == -1) {
            cerr << "There was an error creating the directory " << outputDirectory << endl; // debug error message
            exit(1);
        }
    }
}

// this function will count the number of alive neighbors around a given cell
inline int countAliveNeighbors(const int *board, int index, int boardSize) {
    int aliveNeighborsCount = 0;
    int row = index / boardSize;
    int col = index % boardSize;

    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (i == 0 && j == 0) {
                continue;
            }
            int newRow = row + i;
            int newCol = col + j;
            if (newRow >= 0 && newRow < boardSize && newCol >= 0 && newCol < boardSize) {
                aliveNeighborsCount += board[newRow * boardSize + newCol];
            }
        }
    }
    return aliveNeighborsCount;
}

// this function will compute the next generation of the board
void nextGeneration(int *current, int *next, int boardSize, int local_rows) {
    for (int i = 1; i < local_rows - 1; ++i) { // skips ghost rows
        for (int j = 0; j < boardSize; ++j) {
            int index = i * boardSize + j;
            int aliveNeighbors = countAliveNeighbors(current, index, boardSize);
            next[index] = (current[index] == 1) ? (aliveNeighbors < 2 || aliveNeighbors > 3 ? 0 : 1) : (aliveNeighbors == 3 ? 1 : 0);
        }
    }
}

// this function will check if the two boards are the same
bool isBoardSame(const int *board1, const int *board2, int size) {
    for (int i = 0; i < size; ++i) {
        if (board1[i] != board2[i]) {
            return false;
        }
    }
    return true;
}

// this function will write the final board to a file (rank 0 only)
void writingFinalBoardToFile(const int *board, const string &outputDirectory, int generation, int boardSize, int numProcesses) {
    creatingOutputDirectory(outputDirectory);
    
    string fileName = outputDirectory + "/hw4_final_board_5000_procs_" + to_string(numProcesses) + "_gen_" + to_string(generation) + "_testcase.txt";
    ofstream outFile(fileName);
    
    for (int i = 0; i < boardSize; ++i) {
        for (int j = 0; j < boardSize; ++j) {
            outFile << (board[i * boardSize + j] ? '*' : '.') << " ";
        }
        outFile << endl;
    }
    outFile.close();
}


// this function will exchange ghost rows between neighboring processes
void exchangeGhostRows(int *board, int boardSize, int local_rows, int rank, int size) {
    MPI_Status status;

    // exchange ghost rows
    if (rank < size - 1) {
        MPI_Sendrecv(&board[(local_rows - 2) * boardSize], boardSize, MPI_INT, rank + 1, 0, &board[(local_rows - 1) * boardSize], boardSize, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, &status);
    }

    if (rank > 0) {
        MPI_Sendrecv(&board[1 * boardSize], boardSize, MPI_INT, rank - 1, 0, &board[0 * boardSize], boardSize, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &status);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 4) {
        if (rank == 0) {
            cout << "Incorrect usage. Please type '" << argv[0] << " <board size> <max generations> <output directory>'" << endl;
        }
        MPI_Finalize();
        return 1;
    }

    // command line arguments
    string outputDirectory = argv[3];
    int boardSize = stoi(argv[1]);
    int maxAmountGenerations = stoi(argv[2]);

    // row-wise partitioning of the board
    int rows_per_process = boardSize / size;
    int extra_rows = boardSize % size;
    int local_rows = rows_per_process + (rank < extra_rows ? 1 : 0); // assigns extra rows to some processes
    local_rows += 2;                                                 // adds 2 for ghost rows (one for top and one for bottom)

    int *board = new int[local_rows * boardSize]();
    int *nextGenerationBoard = new int[local_rows * boardSize]();

    srand(12345 + rank); // different seed for each process to initialize board

    // random board initialization
    for (int i = 1; i < local_rows - 1; ++i) {
        for (int j = 0; j < boardSize; ++j) {
            board[i * boardSize + j] = rand() % 2;
        }
    }

    auto start = high_resolution_clock::now();

    int generation = 0;
    bool noChange = false;

    while (generation < maxAmountGenerations && !noChange)
    {
        // exchange ghost rows with neighboring processes
        exchangeGhostRows(board, boardSize, local_rows, rank, size);

        // what's the next gen
        nextGeneration(board, nextGenerationBoard, boardSize, local_rows);

        // was there a change or no
        noChange = isBoardSame(board, nextGenerationBoard, local_rows * boardSize);

        // check if all processes have no change
        int globalNoChange;
        MPI_Allreduce(&noChange, &globalNoChange, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
        noChange = globalNoChange;

        swap(board, nextGenerationBoard);
        generation++;
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    if (rank == 0)
    {
        cout << "Simulation completed in " << generation << " generations and took " << duration.count() << " ms." << endl;
    }

    // gather all local boards at process 0
    if (rank == 0) {
        int *global_board = new int[boardSize * boardSize]; 

        // prepare for MPI_Gatherv by storing the number of elements to be sent to each process and the displacement
        int *recvcounts = new int[size];
        int *displs = new int[size];

        int offset = 0;
        for (int i = 0; i < size; ++i) {
            int rows_for_process = rows_per_process + (i < extra_rows ? 1 : 0);
            recvcounts[i] = rows_for_process * boardSize; 
            displs[i] = offset;                           
            offset += recvcounts[i];
        }

        // gathers the data from all processes (skip ghost rows)
        MPI_Gatherv(&board[boardSize], (local_rows - 2) * boardSize, MPI_INT, global_board, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);

        writingFinalBoardToFile(global_board, outputDirectory, generation, boardSize, size); // writes to the final board

        delete[] global_board;
        delete[] recvcounts;
        delete[] displs;
    }
    else {
        MPI_Gatherv(&board[boardSize], (local_rows - 2) * boardSize, MPI_INT, nullptr, nullptr, nullptr, MPI_INT, 0, MPI_COMM_WORLD);
    }

    delete[] board;
    delete[] nextGenerationBoard;

    MPI_Finalize();
    return 0;
}
