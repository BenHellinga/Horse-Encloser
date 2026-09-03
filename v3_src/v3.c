#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "solver.h"
#include "printing.h"

#include "v3.h"



/*
    Same algorithm as v2 however focuses on graph optimization. This time, Board is read from the file, then later converted to a Graph and optimized.
    Single tile hallways are marked unplaceable, and consecutive tiles are merged together as either walls cannot be placed to separate them. This reduces the work of BFS over many iterations.
*/



// main



int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filepath>\n", argv[0]);
        return 1;
    }
    char* filepath = argv[1];

    printf("Parsing Board\n");

    Board* board;
    int ret = boardFromFile(filepath, &board);
    if (ret == -1) return 1;

    printf("Board Parsed\n\n");
    printBoard(board);
    printf("\nStarting Solve\n\n");

    Result* result = solve(board);

    printf("\n");

    if (result->solutionFound)
    {
        // place walls on the board for a final visual, board is freed right after so no need to undo
        for (uint8_t i = 0; i < result->numWalls; ++i)
            board->tiles[result->walls[i]] = TILE_WALL;

        printBoard(board);
        printf("\nscore: %d, walls used: %u\n", result->score, result->numWalls);
    }
    else printf("no solution found\n");

    printf("\nCleaning Up\n");

    freeBoard(board);
    freeResult(result);

    printf("Finished\n");
    return 0;
}