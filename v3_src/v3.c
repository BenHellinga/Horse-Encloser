#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

    Board* board;
    ReturnCode ret = boardFromFile(filepath, &board);
    if (ret == ERROR) return 1;

    printBoard(board);

    freeBoard(board);
    return 0;
}