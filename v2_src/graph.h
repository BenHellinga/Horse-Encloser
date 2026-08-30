#ifndef GRAPH_H_
#define GRAPH_H_

#include <stdint.h>
#include <stdlib.h>



// node id



typedef uint16_t NodeID;



// tile type



typedef uint8_t TileType;
enum
{
    TILE_EMPTY,
    TILE_HORSE,
    TILE_UNICORN,
    TILE_WALL,
    TILE_WATER,
    TILE_BEE,
    TILE_CHERRY,
    TILE_APPLE,
    TILE_BLUE_PORTAL,
    TILE_PINK_PORTAL,
    TILE_PURPLE_PORTAL,

    NUM_TILE_TYPES
};



// solid tiles block movement, empty tiles are wallable
#define TILE_IS_SOLID(type) ((type) == TILE_WALL || (type) == TILE_WATER)
#define TILE_IS_EMPTY(type) ((type) == TILE_EMPTY)



// tile chars



#define TILE_CHAR_EMPTY         ' '
#define TILE_CHAR_HORSE         'H'
#define TILE_CHAR_UNICORN       'U'
#define TILE_CHAR_WALL          '#'
#define TILE_CHAR_WATER         '~'
#define TILE_CHAR_BEE           'B'
#define TILE_CHAR_CHERRY        'C'
#define TILE_CHAR_APPLE         'A'
#define TILE_CHAR_BLUE_PORTAL   '1'
#define TILE_CHAR_PINK_PORTAL   '2'
#define TILE_CHAR_PURPLE_PORTAL '3'



// tile lookup table initializers



#define TILE_TYPES_TO_CHAR                          \
{                                                   \
    [TILE_EMPTY]         = TILE_CHAR_EMPTY,         \
    [TILE_HORSE]         = TILE_CHAR_HORSE,         \
    [TILE_UNICORN]       = TILE_CHAR_UNICORN,       \
    [TILE_WALL]          = TILE_CHAR_WALL,          \
    [TILE_WATER]         = TILE_CHAR_WATER,         \
    [TILE_BEE]           = TILE_CHAR_BEE,           \
    [TILE_CHERRY]        = TILE_CHAR_CHERRY,        \
    [TILE_APPLE]         = TILE_CHAR_APPLE,         \
    [TILE_BLUE_PORTAL]   = TILE_CHAR_BLUE_PORTAL,   \
    [TILE_PINK_PORTAL]   = TILE_CHAR_PINK_PORTAL,   \
    [TILE_PURPLE_PORTAL] = TILE_CHAR_PURPLE_PORTAL, \
}
extern const char TILE_CHARS[NUM_TILE_TYPES];



#define TILE_TYPES_TO_POINTS    \
{                               \
    [TILE_EMPTY]         = 1,   \
    [TILE_HORSE]         = 1,   \
    [TILE_UNICORN]       = 1,   \
    [TILE_WALL]          = 0,   \
    [TILE_WATER]         = 0,   \
    [TILE_BEE]           = -4,  \
    [TILE_CHERRY]        = 4,   \
    [TILE_APPLE]         = 11,  \
    [TILE_BLUE_PORTAL]   = 1,   \
    [TILE_PINK_PORTAL]   = 1,   \
    [TILE_PURPLE_PORTAL] = 1,   \
}
extern const int8_t TILE_POINTS[NUM_TILE_TYPES];



// gamemode



typedef uint8_t GamemodeType;
enum
{
    GAMEMODE_CLASSIC,
    GAMEMODE_COSTLY,
    GAMEMODE_LOVEBIRDS,
    GAMEMODE_QUARREL,

    NUM_GAMEMODES
};



// lovers and quarrel require a unicorn tile, classic and costly do not
#define GAMEMODE_REQUIRES_UNICORN(mode) ((mode) == GAMEMODE_LOVEBIRDS || (mode) == GAMEMODE_QUARREL)



#define GAMEMODE_STRING_CLASSIC   "classic"
#define GAMEMODE_STRING_COSTLY    "costly"
#define GAMEMODE_STRING_LOVEBIRDS "lovebirds"
#define GAMEMODE_STRING_QUARREL   "quarrel"



#define GAMEMODE_TYPES_TO_STRING                      \
{                                                     \
    [GAMEMODE_CLASSIC]   = GAMEMODE_STRING_CLASSIC,   \
    [GAMEMODE_COSTLY]    = GAMEMODE_STRING_COSTLY,    \
    [GAMEMODE_LOVEBIRDS] = GAMEMODE_STRING_LOVEBIRDS, \
    [GAMEMODE_QUARREL]   = GAMEMODE_STRING_QUARREL,   \
}
extern const char* GAMEMODE_STRINGS[NUM_GAMEMODES];



// node



#define MAX_EDGES 5



typedef struct
{
    NodeID id;
    TileType type;
    int8_t points;
    uint8_t numEdges;
    NodeID edges[MAX_EDGES];
}
Node;



#define NULL_NODE_ID ((NodeID)-1)
#define IS_NODE_NULL(node) ((node).id == NULL_NODE_ID)



// graph



#define NUM_PORTAL_PAIRS 3



typedef struct
{
    GamemodeType gamemode;
    uint8_t numWalls;
    uint8_t width;
    uint8_t height;
    NodeID numTiles;
    NodeID horse;
    NodeID unicorn;
    Node* nodes;
}
Graph;



// graph.nodes is heap allocated, every NEW_GRAPH needs a matching FREE_GRAPH
#define NEW_GRAPH { 0 }
#define FREE_GRAPH(graph) free((graph).nodes)



#endif