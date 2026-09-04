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
static char bfsStatePtrToChar(void* object, void* context);
static char endTypePtrToChar(void* object, void* context);

static char nodePrintingInfoToTypeChar(NodePrintingInfo* info);
static char nodePrintingInfoToEdgeChar(NodePrintingInfo* info);
static char nodePrintingInfoToValueChar(NodePrintingInfo* info);
static char nodePrintingInfoToEndTypeChar(NodePrintingInfo* info, Graph* graph);

static char nodeTypeToChar(NodeType type);
static char nodeIDToEndTypeChar(Graph* graph, NodeID id);
static char endTypeToChar(EndType type);



// header functions



GraphPrintingInfo* getGraphPrintingInfo(Graph* graph, Board* board)
{
    GraphPrintingInfo* info = (GraphPrintingInfo*)calloc(1, sizeof(GraphPrintingInfo));

    NodeCount numNodes = board->numTiles;

    NodePrintingInfo* nodes = (NodePrintingInfo*)calloc(numNodes, sizeof(NodePrintingInfo));
    NodeGroup* groups = (NodeGroup*)calloc(numNodes, sizeof(NodeGroup));
    char* charBuffer = (char*)calloc(((board->width * 3 + 4) * 2 + 1) * (board->height + 2) + 1, sizeof(char));

    for (NodeID n = 0; n < numNodes; n++)
        nodes[n] = (NodePrintingInfo){ .status = NPS_NODE, .node = &graph->nodes[n], .group = NULL, .printGroupLabel = true };

    *info = (GraphPrintingInfo)
    {
        .graph = graph,
        .width = board->width,
        .height = board->height,
        .numNodes = numNodes,
        .nodes = nodes,
        .numGroups = 0,
        .groups = groups,
        .charBuffer = charBuffer,
    };

    return info;
}



void freeGraphPrintingInfo(GraphPrintingInfo* info)
{
    for (NodeCount g = 0; g < info->numGroups; g++)
        if (info->groups[g].id != NULL_ID)
            free(info->groups[g].groupedIDs);

    free(info->nodes);
    free(info->groups);
    free(info);
}



void printBoard(Board* board)
{
    print2D(board->width, board->height, board->tiles, sizeof(TileType), tileTypeToChar, NULL, true);
    printf("Gamemode: %s\n", GAMEMODE_INFO[board->gamemode].string);
    printf("Walls: %d\n", board->numWalls);
}



void printBFSVisited(GraphPrintingInfo* info, BFSData* data)
{
    print2D(info->width, info->height, data->visited, sizeof(BFSState), bfsStatePtrToChar, &data->state, true);
}



void printEndMask(GraphPrintingInfo* info, EndType* mask)
{
    print2D(info->width, info->height, mask, sizeof(EndType), endTypePtrToChar, NULL, true);
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
        for (int i = 0; i < width * 2 + 1; i++)
            printf("-");
        printf("+\n");
    }

    // print each row, converting each cell to a char
    for (uint8_t y = 0; y < height; y++)
    {
        if (border)
            printf("| ");

        for (uint8_t x = 0; x < width; x++)
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
        for (int i = 0; i < width * 2 + 1; i++)
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



static char bfsStatePtrToChar(void* object, void* context)
{
    return *(BFSState*)object == *(BFSState*)context ? EMPTY_CHAR : NULL_CHAR;
}



static char endTypePtrToChar(void* object, void* context)
{
    return endTypeToChar(*(EndType*)object);
}



// graph printing



void printAllGraphInfo(GraphPrintingInfo* info)
{
    #define NUM_PANELS 4

    NodePrintingInfo* nodes = info->nodes;
    int width = info->width;
    int height = info->height;

    // one leading space, then each char is followed by a space
    int panelContentWidth = width * 2 + 1;

    // top border
    for (int panel = 0; panel < NUM_PANELS; panel++)
    {
        printf("+");
        for (int i = 0; i < panelContentWidth; i++)
            printf("-");
    }
    printf("+\n");

    // content rows
    for (int y = 0; y < height; y++)
    {
        for (int panel = 0; panel < NUM_PANELS; ++panel)
        {
            printf("| ");

            for (int x = 0; x < width; x++)
            {
                NodePrintingInfo* node = &nodes[y * width + x];
                char c;
                switch (panel)
                {
                    case 0: c = nodePrintingInfoToTypeChar(node); break;
                    case 1: c = nodePrintingInfoToEdgeChar(node); break;
                    case 2: c = nodePrintingInfoToValueChar(node); break;
                    case 3: c = nodePrintingInfoToEndTypeChar(node, info->graph); break;
                }
                printf("%c ", c);
            }
        }
        printf("|\n");
    }

    // bottom border
    for (int panel = 0; panel < NUM_PANELS; panel++)
    {
        printf("+");
        for (int i = 0; i < panelContentWidth; i++)
            printf("-");
    }
    printf("+\n");

    NodeCount numGroups = info->numGroups;
    NodeGroup* groups = info->groups;

    // print group info
    for (NodeCount g = 0; g < numGroups; g++)
    {
        NodeGroup* group = &groups[g];
        if (group->id == NULL_ID) continue;

        char groupChar = group->id + 'A';
        Node* node = group->node;

        for (int panel = 0; panel < NUM_PANELS; panel++)
        {
            printf(panel == 0 ? "|" : " ");

            char valueStr[8];
            switch (panel)
            {
                case 0: snprintf(valueStr, sizeof(valueStr), "%c", nodeTypeToChar(node->type)); break;
                case 1: snprintf(valueStr, sizeof(valueStr), "%u", node->numEdges); break;
                case 2: snprintf(valueStr, sizeof(valueStr), "%d", node->value); break;
                case 3: snprintf(valueStr, sizeof(valueStr), "%c", nodeIDToEndTypeChar(info->graph, node->id)); break;
            }

            char field[32];
            snprintf(field, sizeof(field), " %c:   %s", groupChar, valueStr);
            printf("%-*s", panelContentWidth, field);
        }
        printf("|\n");
    }

    // closing border for the group table
    if (numGroups > 0)
    {
        int tableWidth = NUM_PANELS * (panelContentWidth + 1) - 1;
        printf("+");
        for (int i = 0; i < tableWidth; i++)
            printf("-");
        printf("+\n");
    }
}



static char nodePrintingInfoToTypeChar(NodePrintingInfo* info)
{
    if (info->status == NPS_NULL) return NULL_CHAR;

    if (info->status == NPS_GROUP)
    {
        if (info->printGroupLabel)
            return info->group->id + 'A';
        return NULL_CHAR;
    }

    Node* node = info->node;
    if (node->id == NULL_ID) return NULL_ID_CHAR;

    return nodeTypeToChar(node->type);
}



static char nodePrintingInfoToEdgeChar(NodePrintingInfo* info)
{
    if (info->status == NPS_NULL) return NULL_CHAR;

    if (info->status == NPS_GROUP)
    {
        if (info->printGroupLabel)
            return info->group->id + 'A';
        return NULL_CHAR;
    }

    Node* node = info->node;
    if (node->id == NULL_ID) return NULL_ID_CHAR;

    return node->numEdges + '0';
}



static char nodePrintingInfoToValueChar(NodePrintingInfo* info)
{
    if (info->status == NPS_NULL) return NULL_CHAR;

    if (info->status == NPS_GROUP)
    {
        if (info->printGroupLabel)
            return info->group->id + 'A';
        return NULL_CHAR;
    }

    Node* node = info->node;
    if (node->id == NULL_ID) return NULL_ID_CHAR;

    Score value = node->value;

    if (value == -4) return 'b';
    if (value == 11) return 'a';
    return value + '0';
}



static char nodePrintingInfoToEndTypeChar(NodePrintingInfo* info, Graph* graph)
{
    if (info->status == NPS_NULL) return NULL_CHAR;

    if (info->status == NPS_GROUP)
    {
        if (info->printGroupLabel)
            return info->group->id + 'A';
        return NULL_CHAR;
    }

    Node* node = info->node;
    if (node->id == NULL_ID) return NULL_ID_CHAR;

    return nodeIDToEndTypeChar(graph, node->id);
}



static char nodeTypeToChar(NodeType type)
{
    switch (type)
    {
        case NODE_NONE:         return RESTRICTION_NONE_CHAR;
        case NODE_UNPLACEABLE:  return RESTRICTION_UNPLACEABLE_CHAR;
        case NODE_UNWALKABLE:   return RESTRICTION_UNWALKABLE_CHAR;
        default:                return UNKNOWN_CHAR; 
    }
}



static char nodeIDToEndTypeChar(Graph* graph, NodeID id)
{
    if (graph->horse != NULL_ID)
        if (graph->horse == id)
            return START_HORSE_CHAR;
    
    if (graph->unicorn != NULL_ID)
        if (graph->unicorn == id)
            return START_UNICORN_CHAR;

    NodeCount numEnds = graph->numOptional;
    NodeID* ends = graph->optionalEnds;

    for (NodeCount e = 0; e < numEnds; e++)
        if (ends[e] == id)
            return endTypeToChar(END_OPTIONAL);

    numEnds = graph->numRequired;
    ends = graph->requiredEnds;

    for (NodeCount e = 0; e < numEnds; e++)
        if (ends[e] == id)
            return endTypeToChar(END_REQUIRED);

    return endTypeToChar(END_NONE);
}



static char endTypeToChar(EndType type)
{
    switch (type)
    {
        case END_NONE:      return END_NONE_CHAR;
        case END_OPTIONAL:  return END_OPTIONAL_CHAR;
        case END_REQUIRED:  return END_REQUIRED_CHAR;
        default:            return UNKNOWN_CHAR;
    }
}