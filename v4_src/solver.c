#include <stdlib.h>
#include <stdio.h>

#include "printing.h"

#include "solver.h"



// defines



typedef struct PathBlockerNodeStruct
{
    NodeID blocker;
    NodeID* path;

    NodeID headNode;
    NodeID tailNode;
    struct PathBlockerNodeStruct* head;
    struct PathBlockerNodeStruct* tail;
    struct PathBlockerNodeStruct** children;

    struct PathBlockerNodeStruct* next;
}
PathBlockerNode;



// global



// declarations



static Result* findOptimalEnclosement(Graph* graph, uint8_t numWalls);



// header functions



// solves the board, puts the best solution in result, otherwise returns -1 if no solution found
Result* solve(Board* board)
{
    printf("Converting to Graph\n");

    // generate and optimize graph from board
    Graph* graph = graphFromBoard(board);
    printGraphInfo(graph, GRAPH_PRINT_ALL);

    printf("\nBoard Converted\n");

    // iterative deepening to get a sense of progress

    findOptimalEnclosement(graph, 1);

    // cleanup

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



// solving



static Result* findOptimalEnclosement(Graph* graph, uint8_t numWalls)
{
    // setup
    // BoundaryPath LinkedList
    // boundary counts

    // BFS would take boundary counts (for walkability and endpoints) and BoundaryPath LinkedList (for startpoints) 

    // enclosed = false
    // main loop
        // if enclosed
            // move first boundary forward by 1
            
            // changed1 = true
            // changed2 = true
            // while (changed1 || changed2)
                // if changed1, changed2 |= checkTrivialBoundaryMoves, changed1 = false
                // if changed2, changed1 |= recomputeBlockedPaths, changed2 = false

            // enclosed/next path = BFS

            // if enclosed
                // compare with current best and record if better (value primary, numWalls secondary)
                // continue
            // else
                // 

    // cleanup

    return NULL;
}