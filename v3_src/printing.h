#ifndef PRINTING_H_
#define PRINTING_H_

#include "board.h"
#include "graph.h"
#include "bfs.h"



// typedefs



typedef struct
{
    NodeID id;
    Node* node;
    NodeCount numGroupedIDs;
    NodeID* groupedIDs;
}
NodeGroup;



typedef uint8_t NodePrintingStatus;
enum
{
    NPS_NULL,
    NPS_NODE,
    NPS_GROUP,
};



typedef struct
{
    NodePrintingStatus status;
    Node* node;
    NodeGroup* group;
    bool printGroupLabel;
}
NodePrintingInfo;



typedef struct
{
    Graph* graph;
    uint8_t width;
    uint8_t height;
    NodeCount numNodes;
    NodePrintingInfo* nodes;
    NodeCount numGroups;
    NodeGroup* groups;
    char* charBuffer;
}
GraphPrintingInfo;



#define UNKNOWN_CHAR                    '?'
#define NULL_CHAR                  ' '
#define NULL_ID_CHAR                    'n'
#define EMPTY_CHAR                      '.'
#define OCCUPIED_CHAR                   'x'
#define SOLID_CHAR                      '#'

#define RESTRICTION_NONE_CHAR           EMPTY_CHAR
#define RESTRICTION_UNPLACEABLE_CHAR    OCCUPIED_CHAR
#define RESTRICTION_UNWALKABLE_CHAR     SOLID_CHAR

#define END_NONE_CHAR                   EMPTY_CHAR
#define END_OPTIONAL_CHAR               'o'
#define END_REQUIRED_CHAR               'r'

#define START_HORSE_CHAR                'h'
#define START_UNICORN_CHAR              'u'



// declarations



GraphPrintingInfo* getGraphPrintingInfo(Graph* graph, Board* board);
void freeGraphPrintingInfo(GraphPrintingInfo* info);

void printBoard(Board* board);
void printBFSVisited(GraphPrintingInfo* info, BFSData* data);
void printEndMask(GraphPrintingInfo* info, EndType* mask);

void printAllGraphInfo(GraphPrintingInfo* info);



#endif