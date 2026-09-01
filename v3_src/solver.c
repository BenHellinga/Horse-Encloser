#include <stdlib.h>
#include <stdio.h>

#include "graph.h"

#include "solver.h"



// defines



// global



// declarations



// header functions



// solves the board, puts the best solution in result, otherwise returns -1 if no solution found
Result* solve(Board* board)
{
    Graph* graph = graphFromBoard(board);

    freeGraph(graph);

    Result* result = (Result*)calloc(1, sizeof(Result));
    result->walls = (NodeID*)calloc(1, sizeof(NodeID));

    return result;
}



// frees a result
void freeResult(Result* result)
{
    free(result->walls);
    free(result);
}