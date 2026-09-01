#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "solver.h"
#include "printing.h"

#include "v3.h"



/*
    TODO
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
    printf("\nStarting Solve\n");

    Result* result = solve(board);

    printf("Cleaning Up\n");

    freeBoard(board);
    freeResult(result);

    printf("Finished\n");
    return 0;
}