#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"



// defines



#define BOARD_LINE_BUFFER_SIZE 256



// global



const TileInfo TILE_INFO[] = DEFINED_TILE_INFO;
const GamemodeInfo GAMEMODE_INFO[] = DEFINED_GAMEMODE_INFO;



// declarations



static int parseBoardVersion1(FILE* file, Board** board);

static int readVersionNumber(FILE* file, int* version);
static int readGamemode(FILE* file, Board* board);
static int readNumWalls(FILE* file, Board* board);
static int readBoardSize(FILE* file, Board* board);
static int readBoardTiles(FILE* file, Board* board);

static int tryGetTileTypeFromChar(char c, TileType* tileType);
static int tryGetGamemodeFromString(char* str, Gamemode* gamemode);

static int validateBoard(Board* board);



// header functions



// parses the board from the provided file
int boardFromFile(char* filepath, Board** board)
{
    int ret;

    // open the file
    FILE* file = fopen(filepath, "r");
    if (!file)
    {
        printf("Error: Could not open '%s'\n", filepath);
        return -1;
    }

    // get version number
    int version;
    ret = readVersionNumber(file, &version);
    if (ret == -1)
    {
        fclose(file);
        return -1;
    }

    switch (version)
    {
        case 1:
            ret = parseBoardVersion1(file, board);
            break;

        default:
            printf("Error: Unsupported version number\n");
            ret = -1;
            break;
    }

    fclose(file);

    if (ret == -1) return -1;

    // validate the parsed board, regardless of which version parsed it
    ret = validateBoard(*board);
    if (ret == -1)
    {
        freeBoard(*board);
        return -1;
    }

    return 0;
}



// frees a board pointer
void freeBoard(Board* board)
{
    free(board->tiles);
    free(board);
}



// version 1



// parses a version 1 board file (already past the version line) into a Board
static int parseBoardVersion1(FILE* file, Board** board)
{
    int ret;

    Board* parsedBoard = (Board*)calloc(1, sizeof(Board));

    // read gamemode
    ret = readGamemode(file, parsedBoard);
    if (ret == -1)
    {
        free(parsedBoard);
        return -1;
    }

    // read num walls
    ret = readNumWalls(file, parsedBoard);
    if (ret == -1)
    {
        free(parsedBoard);
        return -1;
    }

    // read board size
    ret = readBoardSize(file, parsedBoard);
    if (ret == -1)
    {
        free(parsedBoard);
        return -1;
    }

    parsedBoard->tiles = (TileType*)calloc(parsedBoard->numTiles, sizeof(TileType));

    // read tiles
    ret = readBoardTiles(file, parsedBoard);
    if (ret == -1)
    {
        free(parsedBoard);
        return -1;
    }

    *board = parsedBoard;
    return 0;
}



// helpers



// reads the version number
static int readVersionNumber(FILE* file, int* version)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read version number\n");
        return -1;
    }

    if (sscanf(buffer, "%d", version) != 1)
    {
        printf("Error: Could not read version number\n");
        return -1;
    }

    return 0;
}



// reads which gamemode the board uses
static int readGamemode(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read gamemode\n");
        return -1;
    }

    // strip trailing newline/carriage return
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        buffer[--len] = '\0';

    Gamemode gamemode;
    if (tryGetGamemodeFromString(buffer, &gamemode) == -1)
    {
        printf("Error: Unknown gamemode\n");
        return -1;
    }

    board->gamemode = gamemode;

    return 0;
}



// reads the number of walls allowed for this puzzle
static int readNumWalls(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read number of walls\n");
        return -1;
    }

    int numWalls;
    if (sscanf(buffer, "%d", &numWalls) != 1)
    {
        printf("Error: Could not read number of walls\n");
        return -1;
    }

    if (numWalls < 0 || numWalls > UINT8_MAX)
    {
        printf("Error: Invalid number of walls\n");
        return -1;
    }

    board->numWalls = (uint8_t)numWalls;

    return 0;
}



// reads the width and height of the board
static int readBoardSize(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read board size\n");
        return -1;
    }

    int width, height;
    if (sscanf(buffer, "%d , %d", &width, &height) != 2)
    {
        printf("Error: Could not read board size\n");
        return -1;
    }

    if (width <= 0 || width > UINT8_MAX || height <= 0 || height > UINT8_MAX)
    {
        printf("Error: Invalid board size\n");
        return -1;
    }

    board->width = (uint8_t)width;
    board->height = (uint8_t)height;
    board->numTiles = (uint16_t)(width * height);

    return 0;
}



// read the board tiles from the file
static int readBoardTiles(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];

    for (int y = 0; y < board->height; y++)
    {
        if (!fgets(buffer, sizeof(buffer), file))
        {
            printf("Error: Could not read board row %d\n", y);
            return -1;
        }

        // strip trailing newline/carriage return
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        if (len < board->width)
        {
            printf("Error: Board row %d is too short\n", y);
            return -1;
        }

        TileType* tiles = (TileType*)board->tiles;
        for (int x = 0; x < board->width; x++)
        {
            TileType tile;
            if (tryGetTileTypeFromChar(buffer[x], &tile) == -1)
            {
                printf("Error: Unknown tile\n");
                return -1;
            }

            tiles[y * board->width + x] = tile;
        }
    }

    return 0;
}



// converters



// tries to convert a char to a TileType using TILE_INFO
static int tryGetTileTypeFromChar(char c, TileType* tileType)
{
    for (int i = 0; i < NUM_TILE_TYPES; i++)
    {
        if (TILE_INFO[i].c == c)
        {
            *tileType = (TileType)i;
            return 0;
        }
    }

    return -1;
}



// tries to convert a string to a Gamemode using GAMEMODE_INFO
static int tryGetGamemodeFromString(char* str, Gamemode* gamemode)
{
    for (int i = 0; i < NUM_GAMEMODES; i++)
    {
        if (strcmp(GAMEMODE_INFO[i].string, str) == 0)
        {
            *gamemode = (Gamemode)i;
            return 0;
        }
    }

    return -1;
}



// validation



// checks that portals come in pairs, and that the horse and unicorn exist when they should depending on the gamemode
static int validateBoard(Board* board)
{
    int portalCounts[NUM_PORTAL_PAIRS] = { 0 };
    bool hasHorse = false;
    bool hasUnicorn = false;

    int numTiles = board->numTiles;
    for (int i = 0; i < numTiles; i++)
    {
        TileType tile = board->tiles[i];

        if (TILE_IS_PORTAL(tile))
        {
            // check too many portals
            if (portalCounts[PORTAL_PAIR(tile)] == 2)
            {
                printf("Error: Too many '%c' portals\n", TILE_INFO[tile].c);
                return -1;
            }
            ++portalCounts[PORTAL_PAIR(tile)];
        }
        else if (tile == TILE_HORSE)
        {
            // check too many horses
            if (hasHorse)
            {
                printf("Error: Multiple horses found\n");
                return -1;
            }
            hasHorse = true;
        }
        else if (tile == TILE_UNICORN)
        {
            // check too many unicorns
            if (hasUnicorn)
            {
                printf("Error: Multiple unicorns found\n");
                return -1;
            }
            hasUnicorn = true;
        }
    }

    // check too few portals
    for (int i = 0; i < NUM_PORTAL_PAIRS; i++)
    {
        if (portalCounts[i] == 1)
        {
            printf("Error: Too few '%c' portals\n", TILE_INFO[TILE_BLUE_PORTAL + i].c);
            return -1;
        }
    }

    // existence of the horse/unicorn depends on the gamemode
    const GamemodeInfo* gamemodeInfo = &GAMEMODE_INFO[board->gamemode];

    if (hasHorse != gamemodeInfo->horse)
    {
        if (gamemodeInfo->horse)
            printf("Error: Gamemode '%s' requires a horse\n", gamemodeInfo->string);
        else
            printf("Error: Horse cannot exist in gamemode '%s'\n", gamemodeInfo->string);
        return -1;
    }

    if (hasUnicorn != gamemodeInfo->unicorn)
    {
        if (gamemodeInfo->unicorn)
            printf("Error: Gamemode '%s' requires a unicorn\n", gamemodeInfo->string);
        else
            printf("Error: Unicorn cannot exist in gamemode '%s'\n", gamemodeInfo->string);
        return -1;
    }

    return 0;
}