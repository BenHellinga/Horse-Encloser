#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "graph.h"

#include "printing.h"



// defines



typedef char (*ObjectToCharFunction)(void* object, void* context);



// declarations



static void print2D(uint8_t width, uint8_t height, void* grid, size_t elementSize, ObjectToCharFunction toChar, void* context, bool border);

static char tileTypeToChar(void* object, void* context);
static char nodePointerToTypeChar(void* object, void* context);
static char nodePointerToEdgeChar(void* object, void* context);
static char bfsStateToChar(void* object, void* context);



// header functions



GraphPrintingInfo* getGraphPrintingInfo(Graph* graph, Board* board)
{
    GraphPrintingInfo* info = (GraphPrintingInfo*)calloc(1, sizeof(GraphPrintingInfo));

    NodeCount numNodes = board->numTiles;

    Node** nodes = (Node**)calloc(numNodes, sizeof(Node*));
    for (NodeID n = 0; n < numNodes; ++n)
        nodes[n] = &graph->nodes[n];

    info->graph = graph;
    info->width = board->width;
    info->height = board->height;
    info->numNodes = numNodes;
    info->nodes = nodes;

    return info;
}



void freeGraphPrintingInfo(GraphPrintingInfo* info)
{
    free(info->nodes);
    free(info);
}



void printBoard(Board* board)
{
    print2D(board->width, board->height, board->tiles, sizeof(TileType), tileTypeToChar, NULL, true);
    printf("Gamemode: %s\n", GAMEMODE_INFO[board->gamemode].string);
    printf("Walls: %d\n", board->numWalls);
}



void printGraphTypes(GraphPrintingInfo* info)
{
    print2D(info->width, info->height, info->nodes, sizeof(Node*), nodePointerToTypeChar, NULL, true);
}



void printGraphEdges(GraphPrintingInfo* info)
{
    print2D(info->width, info->height, info->nodes, sizeof(Node*), nodePointerToEdgeChar, NULL, true);
}



void printBFSVisited(GraphPrintingInfo* info, BFSData* data)
{
    print2D(info->width, info->height, data->visited, sizeof(BFSState), bfsStateToChar, &data->state, true);
}



// general printing



// prints a generic flat-2d array
static void print2D(uint8_t width, uint8_t height, void* grid, size_t elementSize, ObjectToCharFunction toChar, void* context, bool border)
{
    uint8_t* bytes = (uint8_t*)grid;

    // top border
    if (border)
    {
        printf("+");
        for (int i = 0; i < width * 2 + 1; ++i)
            printf("-");
        printf("+\n");
    }

    // print each row, converting each cell to a char
    for (uint8_t y = 0; y < height; ++y)
    {
        if (border)
            printf("| ");

        for (uint8_t x = 0; x < width; ++x)
        {
            void* element = bytes + (y * width + x) * elementSize;
            printf("%c ", toChar(element, context));
        }

        if (border)
            printf("|");

        printf("\n");
    }

    // bottom border
    if (border)
    {
        printf("+");
        for (int i = 0; i < width * 2 + 1; ++i)
            printf("-");
        printf("+\n");
    }
}



// converters



static char tileTypeToChar(void* object, void* context)
{
    TileType tile = *(TileType*)object;
    return TILE_INFO[tile].c;
}



static char nodePointerToTypeChar(void* object, void* context)
{
    Node* node = *(Node**)object;

    if (node == NULL) return NULL_NODE_CHAR;
    if (node->id == NULL_ID) return NULL_ID_CHAR;
    
    // switch (node->type)
    // {
    //     case END_OPTIONAL: return END_OPTIONAL_CHAR;
    //     case END_REQUIRED: return END_REQUIRED_CHAR;
    // }

    switch (node->type)
    {
        case NODE_NONE:        return RESTRICTION_NONE_CHAR;
        case NODE_UNPLACEABLE: return RESTRICTION_UNPLACEABLE_CHAR;
        case NODE_UNWALKABLE:  return RESTRICTION_UNWALKABLE_CHAR;
    }

    return UNKNOWN_CHAR;
}



static char nodePointerToEdgeChar(void* object, void* context)
{
    Node* node = *(Node**)object;

    if (node == NULL) return ' ';
    if (node->id == NULL_ID) return 'n';

    return node->numEdges + '0';
}



static char bfsStateToChar(void* object, void* context)
{
    return *(BFSState*)object == *(BFSState*)context ? '.' : ' ';
}