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



#define GRAPH_GROUP_INFO_BUFFER_SIZE   8
#define GRAPH_GROUP_PREFIX_BUFFER_SIZE 8
#define GRAPH_GROUP_FIELD_BUFFER_SIZE  (GRAPH_GROUP_PREFIX_BUFFER_SIZE + GRAPH_GROUP_INFO_BUFFER_SIZE)

typedef char (*GraphPanelCharFunction)(Graph* graph, Node* node);
typedef int  (*GraphGroupInfoFunction)(Graph* graph, Node* node, char* buffer, size_t bufferSize);

typedef struct
{
    GraphPrintFlags        flag;
    const char*            label;
    GraphPanelCharFunction getPanelChar;
    GraphGroupInfoFunction getGroupInfo;
}
GraphPanel;



// declarations



static void print2D(uint8_t width, uint8_t height, void* grid, size_t elementSize, ObjectToCharFunction toChar, void* context, bool border);

// board
static char tileTypeToChar(void* object, void* context);

// graph
static void printGraphBorder(int numPanels, int panelContentWidth);
static Node* graphGetNode(Graph* graph, NodeID id);
static char graphTileToChar(Graph* graph, GraphTile* tile, const GraphPanel* panel);

static char graphPanelCharTile(Graph* graph, Node* node);
static char graphPanelCharEnd(Graph* graph, Node* node);
static char graphPanelCharValue(Graph* graph, Node* node);
static char graphPanelCharEdges(Graph* graph, Node* node);
static char graphPanelCharBoundary(Graph* graph, Node* node);

static int graphGroupInfoTile(Graph* graph, Node* node, char* buffer, size_t bufferSize);
static int graphGroupInfoEnd(Graph* graph, Node* node, char* buffer, size_t bufferSize);
static int graphGroupInfoValue(Graph* graph, Node* node, char* buffer, size_t bufferSize);
static int graphGroupInfoEdges(Graph* graph, Node* node, char* buffer, size_t bufferSize);
static int graphGroupInfoBoundary(Graph* graph, Node* node, char* buffer, size_t bufferSize);



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



// board



void printBoard(Board* board)
{
    print2D(board->width, board->height, board->tiles, sizeof(TileType), tileTypeToChar, NULL, true);
    printf("Gamemode: %s\n", GAMEMODE_INFO[board->gamemode].string);
    printf("Walls: %d\n", board->numWalls);
}



static char tileTypeToChar(void* object, void* context)
{
    TileType tile = *(TileType*)object;
    return TILE_INFO[tile].c;
}



// graph printing



static const GraphPanel graphPanels[NUM_GRAPH_PRINT_FLAGS] =
{
    { GRAPH_PRINT_TILE,     "Tiles",    graphPanelCharTile,     graphGroupInfoTile     },
    { GRAPH_PRINT_EDGES,    "Edges",    graphPanelCharEdges,    graphGroupInfoEdges    },
    { GRAPH_PRINT_VALUE,    "Values",   graphPanelCharValue,    graphGroupInfoValue    },
    { GRAPH_PRINT_END,      "Ends",     graphPanelCharEnd,      graphGroupInfoEnd      },
    { GRAPH_PRINT_BOUNDARY, "Boundary", graphPanelCharBoundary, graphGroupInfoBoundary },
};



void printGraphInfo(Graph* graph, GraphPrintFlags flags)
{
    int width  = graph->width;
    int height = graph->height;

    // one leading space, then each char is followed by a space
    int panelContentWidth = width * 2 + 1;

    int numActivePanels = 0;
    for (int p = 0; p < NUM_GRAPH_PRINT_FLAGS; p++)
        if (flags & graphPanels[p].flag)
            numActivePanels++;

    if (numActivePanels == 0)
        return;

    // label row (loose, no border chars), separated from prior output
    printf("\n");
    for (int p = 0; p < NUM_GRAPH_PRINT_FLAGS; p++)
    {
        const GraphPanel* panel = &graphPanels[p];
        if (!(flags & panel->flag))
            continue;

        printf("  %-*s", panelContentWidth - 1, panel->label);
    }
    printf("\n");

    // top border
    printGraphBorder(numActivePanels, panelContentWidth);

    // content rows
    for (int y = 0; y < height; y++)
    {
        for (int p = 0; p < NUM_GRAPH_PRINT_FLAGS; p++)
        {
            const GraphPanel* panel = &graphPanels[p];
            if (!(flags & panel->flag))
                continue;

            printf("| ");

            for (int x = 0; x < width; x++)
            {
                GraphTile* tile = &graph->tiles[y * width + x];
                char c = graphTileToChar(graph, tile, panel);
                printf("%c ", c);
            }
        }
        printf("|\n");
    }

    // bottom border
    printGraphBorder(numActivePanels, panelContentWidth);

    // print group info
    NodeCount numGroups = graph->numGroups;
    TileGroup* groups = graph->groups;
    bool printedAnyGroup = false;

    // loop over groups
    for (NodeCount g = 0; g < numGroups; g++)
    {
        TileGroup* group = &groups[g];
        if (group->groupID == NULL_ID) continue;

        printedAnyGroup = true;
        char groupChar = (char)(group->groupID + 'A');
        Node* node = graphGetNode(graph, group->nodeID);

        // print group info for this panel
        bool firstPanel = true;
        for (int p = 0; p < NUM_GRAPH_PRINT_FLAGS; p++)
        {
            const GraphPanel* panel = &graphPanels[p];
            if (!(flags & panel->flag))
                continue;

            printf(firstPanel ? "|" : " ");
            firstPanel = false;

            char field[GRAPH_GROUP_FIELD_BUFFER_SIZE];
            int prefixLen = snprintf(field, sizeof(field), " %c:   ", groupChar);
            panel->getGroupInfo(graph, node, field + prefixLen, sizeof(field) - prefixLen);

            printf("%-*s", panelContentWidth, field);
        }
        printf("|\n");
    }

    // closing border for the group table
    if (printedAnyGroup)
    {
        int tableWidth = numActivePanels * (panelContentWidth + 1) - 1;
        printGraphBorder(1, tableWidth);
    }
}



static void printGraphBorder(int numPanels, int panelContentWidth)
{
    for (int p = 0; p < numPanels; p++)
    {
        printf("+");
        for (int i = 0; i < panelContentWidth; i++)
            printf("-");
    }
    printf("+\n");
}



static Node* graphGetNode(Graph* graph, NodeID id)
{
    return &graph->nodes[id];
}



static char graphTileToChar(Graph* graph, GraphTile* tile, const GraphPanel* panel)
{
    if (tile->type == GTT_NULL || tile->noPrint)
        return NULL_CHAR;

    if (tile->type == GTT_GROUP)
        return (char)(tile->group->groupID + 'A');

    // GTT_NODE
    if (tile->nodeID == NULL_ID)
        return NULL_ID_CHAR;

    Node* node = graphGetNode(graph, tile->nodeID);
    return panel->getPanelChar(graph, node);
}



static char graphPanelCharTile(Graph* graph, Node* node)
{
    (void)graph;

    if (node->flags & FLAG_SOLID)    return SOLID_CHAR;
    if (node->flags & FLAG_OCCUPIED) return OCCUPIED_CHAR;
    return EMPTY_CHAR;
}



static char graphPanelCharEnd(Graph* graph, Node* node)
{
    if (graph->horse   != NULL_ID && graph->horse   == node->nodeID) return START_HORSE_CHAR;
    if (graph->unicorn != NULL_ID && graph->unicorn == node->nodeID) return START_UNICORN_CHAR;

    if (node->flags & FLAG_REQUIRED) return END_REQUIRED_CHAR;
    if (node->flags & FLAG_OPTIONAL) return END_OPTIONAL_CHAR;
    return EMPTY_CHAR;
}



static char graphPanelCharValue(Graph* graph, Node* node)
{
    (void)graph;

    Score value = node->value;
    if (value == -4) return 'b';
    if (value == 11) return 'a';
    return (char)(value + '0');
}



static char graphPanelCharEdges(Graph* graph, Node* node)
{
    (void)graph;
    return (char)(node->edges.num + '0');
}



static char graphPanelCharBoundary(Graph* graph, Node* node)
{
    (void)graph;

    if (node->flags & FLAG_INSIDE)  return BOUNDARY_INSIDE_CHAR;
    if (node->flags & FLAG_OUTSIDE) return BOUNDARY_OUTSIDE_CHAR;
    return EMPTY_CHAR;
}



static int graphGroupInfoTile(Graph* graph, Node* node, char* buffer, size_t bufferSize)
{
    return snprintf(buffer, bufferSize, "%c", graphPanelCharTile(graph, node));
}



static int graphGroupInfoEnd(Graph* graph, Node* node, char* buffer, size_t bufferSize)
{
    return snprintf(buffer, bufferSize, "%c", graphPanelCharEnd(graph, node));
}



static int graphGroupInfoValue(Graph* graph, Node* node, char* buffer, size_t bufferSize)
{
    (void)graph;
    return snprintf(buffer, bufferSize, "%d", node->value);
}



static int graphGroupInfoEdges(Graph* graph, Node* node, char* buffer, size_t bufferSize)
{
    (void)graph;
    return snprintf(buffer, bufferSize, "%u", node->edges.num);
}



static int graphGroupInfoBoundary(Graph* graph, Node* node, char* buffer, size_t bufferSize)
{
    return snprintf(buffer, bufferSize, "%c", graphPanelCharBoundary(graph, node));
}