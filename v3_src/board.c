#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"



// defines



#define BOARD_LINE_BUFFER_SIZE 256



// global



TileInfo TILE_INFO[] = DEFINED_TILE_INFO;
GamemodeInfo GAMEMODE_INFO[] = DEFINED_GAMEMODE_INFO;



// declarations



static ReturnCode parseBoardVersion1(FILE* file, Board** board);

static ReturnCode readVersionNumber(FILE* file, int* version);
static ReturnCode readGamemode(FILE* file, Board* board);
static ReturnCode readNumWalls(FILE* file, Board* board);
static ReturnCode readBoardSize(FILE* file, Board* board);
static ReturnCode readBoardTiles(FILE* file, Board* board);

static ReturnCode tryGetTileTypeFromChar(char c, TileType* tileType);
static ReturnCode tryGetGamemodeFromString(const char* str, Gamemode* gamemode);



// header functions



// makes a new blank board
Board* newBoard(Gamemode gamemode, uint8_t width, uint8_t height)
{
    Board* board = (Board*)calloc(1, sizeof(Board));

    board->gamemode = gamemode;
    board->width = width;
    board->height = height;
    board->tiles = (TileType*)calloc(width * height, sizeof(TileType));

    return board;
}



// frees a board pointer
void freeBoard(Board* board)
{
    free(board->tiles);
    free(board);
}



// parses the board from the provided file
ReturnCode boardFromFile(char* filepath, Board** board)
{
    ReturnCode ret;

    // open the file
    FILE* file = fopen(filepath, "r");
    if (!file)
    {
        printf("Error: Could not open '%s'\n", filepath);
        return ERROR;
    }

    // get version number
    int version;
    ret = readVersionNumber(file, &version);
    if (ret == ERROR)
    {
        fclose(file);
        return ERROR;
    }

    switch (version)
    {
        case 1:
            ret = parseBoardVersion1(file, board);
            break;

        default:
            printf("Error: Unsupported version number\n");
            ret = ERROR;
            break;
    }

    fclose(file);
    return ret;
}



// prints a board to stdout
void printBoard(Board* board)
{
    int lineWidth = board->width * 2 + 1;

    // top border
    printf("+");
    for (int i = 0; i < lineWidth; i++)
        printf("-");
    printf("+\n");

    // rows
    for (int y = 0; y < board->height; y++)
    {
        printf("|");
        for (int x = 0; x < board->width; x++)
            printf(" %c", TILE_INFO[board->tiles[y * board->width + x]].c);
        printf(" |\n");
    }

    // bottom border
    printf("+");
    for (int i = 0; i < lineWidth; i++)
        printf("-");
    printf("+\n");

    printf("Gamemode: %s\n", GAMEMODE_INFO[board->gamemode].string);
    printf("Walls: %d\n", board->numWalls);
}



// version 1



// parses a version 1 board file (already past the version line) into a Board
static ReturnCode parseBoardVersion1(FILE* file, Board** board)
{
    ReturnCode ret;

    Board* parsedBoard = (Board*)calloc(1, sizeof(Board));

    // read gamemode
    ret = readGamemode(file, parsedBoard);
    if (ret == ERROR)
    {
        free(parsedBoard);
        return ERROR;
    }

    // read num walls
    ret = readNumWalls(file, parsedBoard);
    if (ret == ERROR)
    {
        free(parsedBoard);
        return ERROR;
    }

    // read board size
    ret = readBoardSize(file, parsedBoard);
    if (ret == ERROR)
    {
        free(parsedBoard);
        return ERROR;
    }

    parsedBoard->tiles = (TileType*)calloc(parsedBoard->width * parsedBoard->height, sizeof(TileType));

    // read tiles
    ret = readBoardTiles(file, parsedBoard);
    if (ret == ERROR)
    {
        free(parsedBoard->tiles);
        free(parsedBoard);
        return ERROR;
    }

    *board = parsedBoard;
    return SUCCESS;
}



// helpers



// reads the version number
static ReturnCode readVersionNumber(FILE* file, int* version)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read version number\n");
        return ERROR;
    }

    if (sscanf(buffer, "%d", version) != 1)
    {
        printf("Error: Could not read version number\n");
        return ERROR;
    }

    return SUCCESS;
}



// reads which gamemode the board uses
static ReturnCode readGamemode(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read gamemode\n");
        return ERROR;
    }

    // strip trailing newline/carriage return
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        buffer[--len] = '\0';

    Gamemode gamemode;
    if (tryGetGamemodeFromString(buffer, &gamemode) == ERROR)
    {
        printf("Error: Unknown gamemode\n");
        return ERROR;
    }

    board->gamemode = gamemode;

    return SUCCESS;
}



// reads the number of walls allowed for this puzzle
static ReturnCode readNumWalls(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read number of walls\n");
        return ERROR;
    }

    int numWalls;
    if (sscanf(buffer, "%d", &numWalls) != 1)
    {
        printf("Error: Could not read number of walls\n");
        return ERROR;
    }

    if (numWalls < 0 || numWalls > UINT8_MAX)
    {
        printf("Error: Invalid number of walls\n");
        return ERROR;
    }

    board->numWalls = (uint8_t)numWalls;

    return SUCCESS;
}



// reads the width and height of the board
static ReturnCode readBoardSize(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];
    if (!fgets(buffer, sizeof(buffer), file))
    {
        printf("Error: Could not read board size\n");
        return ERROR;
    }

    int width, height;
    if (sscanf(buffer, "%d , %d", &width, &height) != 2)
    {
        printf("Error: Could not read board size\n");
        return ERROR;
    }

    if (width <= 0 || width > UINT8_MAX || height <= 0 || height > UINT8_MAX)
    {
        printf("Error: Invalid board size\n");
        return ERROR;
    }

    board->width = (uint8_t)width;
    board->height = (uint8_t)height;

    return SUCCESS;
}



// read the board tiles from the file
static ReturnCode readBoardTiles(FILE* file, Board* board)
{
    char buffer[BOARD_LINE_BUFFER_SIZE];

    for (int y = 0; y < board->height; y++)
    {
        if (!fgets(buffer, sizeof(buffer), file))
        {
            printf("Error: Could not read board row %d\n", y);
            return ERROR;
        }

        // strip trailing newline/carriage return
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        if (len < board->width)
        {
            printf("Error: Board row %d is too short\n", y);
            return ERROR;
        }

        for (int x = 0; x < board->width; x++)
        {
            TileType tile;
            if (tryGetTileTypeFromChar(buffer[x], &tile) == ERROR)
            {
                printf("Error: Unknown tile\n");
                return ERROR;
            }

            board->tiles[y * board->width + x] = tile;
        }
    }

    return SUCCESS;
}



// converters



// tries to convert a char to a TileType using TILE_INFO
static ReturnCode tryGetTileTypeFromChar(char c, TileType* tileType)
{
    for (int i = 0; i < NUM_TILE_TYPES; i++)
    {
        if (TILE_INFO[i].c == c)
        {
            *tileType = (TileType)i;
            return SUCCESS;
        }
    }

    return ERROR;
}



// tries to convert a string to a Gamemode using GAMEMODE_INFO
static ReturnCode tryGetGamemodeFromString(const char* str, Gamemode* gamemode)
{
    for (int i = 0; i < NUM_GAMEMODES; i++)
    {
        if (strcmp(GAMEMODE_INFO[i].string, str) == 0)
        {
            *gamemode = (Gamemode)i;
            return SUCCESS;
        }
    }

    return ERROR;
}