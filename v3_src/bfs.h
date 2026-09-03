#ifndef BFS_H_
#define BFS_H_

#include <stdbool.h>
#include <stdint.h>

#include "graph.h"



// defines



typedef uint16_t BFSState;

#define MAX_BFS_STATE ((BFSState)UINT16_MAX)



typedef struct
{
    BFSState state;
    NodeCount numNodes;
    BFSState* visited;
    NodeID* queue;
    NodeID* paths;
    NodeCount head;
    NodeCount tail;
}
BFSData;



typedef struct
{
    Graph* graph;
    NodeCount numStarts;
    NodeID* startList;
    EndType* endMask;
    bool stopEarly;
    bool includeEnds;
}
BFSArgs;



typedef struct
{
    bool endReached;
    ScoreValue score;
    NodeID endID;
    NodeCount pathLength;
    NodeID* path;
}
BFSResult;



// declarations



BFSData* initBFSData(NodeCount numNodes);
void freeBFSData(BFSData* bfsData);
BFSArgs* initBFSArgs(Graph* graph);
void freeBFSArgs(BFSArgs* bfsArgs);
BFSResult* initBFSResult(NodeCount numNodes);
void freeBFSResult(BFSResult* bfsResult);

void BFS(BFSData* restrict bfsData, BFSArgs* restrict bfsArgs, BFSResult* restrict bfsResult);



#endif