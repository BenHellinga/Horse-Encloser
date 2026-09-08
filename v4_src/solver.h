#ifndef SOLVER_H_
#define SOLVER_H_

#include <stdint.h>
#include <stdbool.h>

#include "board.h"
#include "graph.h"



// defines



typedef struct
{
    bool solutionFound;
    Score value;
    uint8_t numWalls;
    TileIndex* walls;
}
Result;



// declarations



Result* solve(Board* board);
void freeResult(Result* result);



#endif