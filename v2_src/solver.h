#ifndef SOLVER_H_
#define SOLVER_H_

#include <stdbool.h>
#include <stdlib.h>

#include "graph.h"
#include "printing.h"



// result



typedef struct
{
    bool solutionFound;
    int16_t score;
    NodeID numWalls;
    NodeID* walls;
}
Result;



// result.walls is heap allocated, every NEW_RESULT needs a matching FREE_RESULT
#define NEW_RESULT { 0 }
#define FREE_RESULT(result) free((result).walls)



// functions



void solve(Graph* graph, Result* result);



#endif