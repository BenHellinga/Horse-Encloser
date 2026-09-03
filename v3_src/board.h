#ifndef BOARD_H_
#define BOARD_H_

#include <stdint.h>
#include <stdbool.h>



// defines



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
    TILE_DARK_BLUE_PORTAL,
    TILE_PURPLE_PORTAL,
    TILE_RED_PORTAL,
    TILE_ORANGE_PORTAL,

    NUM_TILE_TYPES
};

#define NUM_PORTAL_PAIRS 6
#define TILE_IS_PORTAL(tile) ((tile) - TILE_BLUE_PORTAL >= 0 && (tile) - TILE_BLUE_PORTAL < NUM_PORTAL_PAIRS)
#define PORTAL_PAIR(tile) ((tile) - TILE_BLUE_PORTAL)

typedef struct
{
    char c;
    int8_t value;
    bool walkable;
    bool placeable;
}
TileInfo;

#define DEFINED_TILE_INFO                                            \
{                            /*   c  value   walkable   placeable */ \
    [TILE_EMPTY]            = { ' ',     1,      true,       true }, \
    [TILE_UNICORN]          = { 'U',     1,      true,      false }, \
    [TILE_HORSE]            = { 'H',     1,      true,      false }, \
    [TILE_WALL]             = { '#',     0,     false,      false }, \
    [TILE_WATER]            = { '~',     0,     false,      false }, \
    [TILE_BEE]              = { 'B',    -4,      true,      false }, \
    [TILE_CHERRY]           = { 'C',     4,      true,      false }, \
    [TILE_APPLE]            = { 'A',    11,      true,      false }, \
    [TILE_BLUE_PORTAL]      = { '1',     1,      true,      false }, \
    [TILE_PINK_PORTAL]      = { '2',     1,      true,      false }, \
    [TILE_DARK_BLUE_PORTAL] = { '3',     1,      true,      false }, \
    [TILE_PURPLE_PORTAL]    = { '4',     1,      true,      false }, \
    [TILE_RED_PORTAL]       = { '5',     1,      true,      false }, \
    [TILE_ORANGE_PORTAL]    = { '6',     1,      true,      false }, \
}
extern const TileInfo TILE_INFO[NUM_TILE_TYPES];



#define GAMEMODE_STRING_BUFFER_SIZE 16

typedef uint8_t Gamemode;
enum
{
    GAMEMODE_CLASSIC,
    GAMEMODE_COSTLY,
    GAMEMODE_QUARREL,
    GAMEMODE_LOVEBIRDS,

    NUM_GAMEMODES
};

typedef struct
{
    char string[GAMEMODE_STRING_BUFFER_SIZE];
    bool horse;
    bool unicorn;
}
GamemodeInfo;

#define DEFINED_GAMEMODE_INFO                                 \
{                         /*      string   horse   unicorn */ \
    [GAMEMODE_CLASSIC]   = {   "classic",   true,    false }, \
    [GAMEMODE_COSTLY]    = {    "costly",   true,    false }, \
    [GAMEMODE_QUARREL]   = {   "quarrel",   true,     true }, \
    [GAMEMODE_LOVEBIRDS] = { "lovebirds",   true,     true }, \
}
extern const GamemodeInfo GAMEMODE_INFO[NUM_GAMEMODES];



typedef struct
{
    Gamemode gamemode;
    uint8_t numWalls;
    uint8_t width;
    uint8_t height;
    uint16_t numTiles;
    TileType* tiles;
}
Board;



// declarations



int boardFromFile(char* filepath, Board** board);
void freeBoard(Board* board);



#endif