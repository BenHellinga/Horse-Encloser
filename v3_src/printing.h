#ifndef PRINTING_H_
#define PRINTING_H_

#include "board.h"
#include "graph.h"
#include "bfs.h"



// typedefs



typedef struct
{
    Graph* graph;
    uint8_t width;
    uint8_t height;
    NodeCount numNodes;
    Node** nodes;
}
GraphPrintingInfo;



#define UNKNOWN_CHAR                 '?'
#define NULL_NODE_CHAR               ' '
#define NULL_ID_CHAR                 'n'
#define RESTRICTION_NONE_CHAR        '.'
#define RESTRICTION_UNPLACEABLE_CHAR 'x'
#define RESTRICTION_UNWALKABLE_CHAR  '#'
#define END_OPTIONAL_CHAR            'o'
#define END_REQUIRED_CHAR            'r'



// declarations



GraphPrintingInfo* getGraphPrintingInfo(Graph* graph, Board* board);
void freeGraphPrintingInfo(GraphPrintingInfo* info);

void printBoard(Board* board);
void printGraphTypes(GraphPrintingInfo* info);
void printGraphEdges(GraphPrintingInfo* info);
void printBFSVisited(GraphPrintingInfo* info, BFSData* data);



#endif