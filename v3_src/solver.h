#ifndef SOLVER_H_
#define SOLVER_H_

#include <stdint.h>
#include <stdbool.h>

#include "board.h"



// defines



typedef uint16_t TileIndex;



typedef struct
{
    bool solutionFound;
    int16_t score;
    uint8_t numWalls;
    TileIndex* walls;
}
Result;



// declarations



Result* solve(Board* board);
void freeResult(Result* result);



#endif