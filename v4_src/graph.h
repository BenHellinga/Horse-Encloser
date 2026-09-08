#ifndef GRAPH_H_
#define GRAPH_H_

#include <stdint.h>

#include "board.h"



// defines



typedef uint16_t NodeID;
typedef uint16_t NodeCount;
typedef int16_t Score;



typedef uint8_t NodeFlags;
enum
{
    FLAG_SOLID    = 1 << 0,
    FLAG_OCCUPIED = 1 << 1,
    FLAG_REQUIRED = 1 << 2,
    FLAG_OPTIONAL = 1 << 3,
    FLAG_INSIDE   = 1 << 4,
    FLAG_OUTSIDE  = 1 << 5,
};



typedef struct
{
    NodeCount num;
    NodeID* list;
}
NodeList;



typedef struct
{
    NodeID groupID;
    NodeID nodeID;
    NodeList labelTiles;
}
TileGroup;



typedef uint8_t GraphTileType;
enum
{
    GTT_NULL,
    GTT_NODE,
    GTT_GROUP,
};



typedef struct
{
    GraphTileType type;
    NodeID nodeID;
    TileGroup* group;
    bool noPrint;
}
GraphTile;



typedef struct
{
    NodeID nodeID;
    NodeID tileID;
    Score value;
    NodeFlags flags;
    NodeList edges;
}
Node;

#define NULL_ID ((NodeID)-1)



typedef struct
{
    NodeCount numNodes;
    Node* nodes;
    NodeCount numEdges;
    NodeID* edges;

    NodeID horse;
    NodeID unicorn;

    uint8_t width;
    uint8_t height;
    uint8_t numWalls;
    Gamemode gamemode;
    GraphTile* tiles;
    NodeCount numGroups;
    TileGroup* groups;

    void* buffer;
}
Graph;



// declarations



Graph* graphFromBoard(Board* board);
void freeGraph(Graph* graph);



#endif