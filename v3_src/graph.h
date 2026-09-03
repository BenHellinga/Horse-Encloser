#ifndef GRAPH_H_
#define GRAPH_H_

#include <stdint.h>

#include "board.h"



// defines



typedef uint16_t NodeID;
typedef uint16_t NodeCount;
typedef int16_t ScoreValue;



typedef uint8_t NodeType;
enum
{
    NODE_NONE,
    NODE_UNPLACEABLE,
    NODE_UNWALKABLE,

    NUM_NODE_TYPES
};



typedef uint8_t EndType;
enum
{
    END_NONE,
    END_OPTIONAL,
    END_REQUIRED,

    NUM_END_TYPES
};



typedef struct
{
    NodeID id;
    ScoreValue value;
    NodeType type;
    NodeCount numEdges;
    NodeID* edges;
}
Node;

#define NULL_ID ((NodeID)-1)



typedef struct
{
    NodeCount numNodes;
    Node* nodes;
    NodeCount numOptional;
    NodeID* optionalEnds;
    NodeCount numRequired;
    NodeID* requiredEnds;
    NodeID horse;
    NodeID unicorn;
}
Graph;



// declarations



Graph* graphFromBoard(Board* board);
void freeGraph(Graph* graph);



#endif