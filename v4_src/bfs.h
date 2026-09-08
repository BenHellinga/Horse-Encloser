#ifndef BFS_H_
#define BFS_H_

#include <stdbool.h>
#include <stdint.h>

#include "graph.h"



// defines



typedef uint16_t BFSState;
#define MAX_BFS_STATE ((BFSState)UINT16_MAX)



// types



// reused buffers, plus outcome of the most recent run
typedef struct
{
    NodeCount numNodes;
    BFSState  state;
    BFSState* visited;
    NodeID*   queue;
    NodeID*   paths;
    NodeCount head;
    NodeCount tail;

    bool   endReached;
    NodeID endID;
    Score  value;
}
BFSData;



// arguments for bfs run
typedef struct
{
    Graph* graph;

    NodeFlags blockFlags;
    NodeFlags endFlags;
    bool stopEarly;
}
BFSArgs;



typedef struct
{
    BFSData data;
    BFSArgs args;
}
BFSContext;



// declarations



BFSContext* initBFSContext(NodeCount numNodes);
void freeBFSContext();

void setBFSArgs(Graph* graph, NodeFlags blockFlags, NodeFlags endFlags, bool stopEarly);
void multiBFS(NodeID* startList, NodeCount numStarts);
void singleBFS(NodeID start);

bool solverBFS();

void recoverPath(NodeID* path, NodeID* start);
void recoverReversePath(NodeID* path, NodeID* start);



#endif