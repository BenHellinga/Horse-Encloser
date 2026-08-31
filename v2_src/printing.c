#include <stdio.h>
#include <stdlib.h>

#include "printing.h"



// functions



static void** buildNodeGrid(const Graph* graph);



// generic 2d printing



void print2D(uint8_t width, uint8_t height, void** grid, ObjectToCharFunction toChar, bool border)
{
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
            printf("%c ", toChar(grid[y * width + x]));

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



// object -> char converters



// prints the tile's own character, blank if the pointer is null
char nodeToChar(const void* object)
{
    if (object == NULL) return ' ';

    const Node* node = (const Node*)object;

    return TILE_CHARS[node->type];
}



// prints the node's edge count, blank if the pointer is null or the node isn't a graph member
char nodeToEdgeCountChar(const void* object)
{
    if (object == NULL)
        return ' ';

    const Node* node = (const Node*)object;

    if (IS_NODE_NULL(*node))
        return ' ';

    return (char)('0' + node->numEdges);
}



// graph printing



// prints the board as tile characters
void printGraphChars(const Graph* graph)
{
    void** grid = buildNodeGrid(graph);

    print2D(graph->width, graph->height, grid, nodeToChar, true);

    free(grid);
}



// prints the board as edge counts, useful for sanity checking graph construction
void printGraphEdges(const Graph* graph)
{
    void** grid = buildNodeGrid(graph);

    print2D(graph->width, graph->height, grid, nodeToEdgeCountChar, true);

    free(grid);
}



// grid helpers



// wraps graph->nodes into a void** grid so it can be passed to print2D
static void** buildNodeGrid(const Graph* graph)
{
    NodeID numTiles = (NodeID)((unsigned int)graph->width * (unsigned int)graph->height);

    void** grid = malloc(sizeof(void*) * numTiles);

    for (NodeID i = 0; i < numTiles; ++i)
        grid[i] = (void*)&graph->nodes[i];

    return grid;
}