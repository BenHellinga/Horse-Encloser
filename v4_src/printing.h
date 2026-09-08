#ifndef PRINTING_H_
#define PRINTING_H_

#include "board.h"
#include "graph.h"
#include "bfs.h"



// defines



#define UNKNOWN_CHAR                    '?'
#define NULL_CHAR                       ' '
#define NULL_ID_CHAR                    'n'

#define EMPTY_CHAR                      '.'
#define OCCUPIED_CHAR                   'x'
#define SOLID_CHAR                      '#'

#define END_OPTIONAL_CHAR               'o'
#define END_REQUIRED_CHAR               'r'
#define START_HORSE_CHAR                'h'
#define START_UNICORN_CHAR              'u'

#define BOUNDARY_INSIDE_CHAR            'i'
#define BOUNDARY_OUTSIDE_CHAR           'o'



typedef uint8_t GraphPrintFlags;
enum
{
    GRAPH_PRINT_TILE     = 1 << 0,
    GRAPH_PRINT_END      = 1 << 1,
    GRAPH_PRINT_VALUE    = 1 << 2,
    GRAPH_PRINT_EDGES    = 1 << 3,
    GRAPH_PRINT_BOUNDARY = 1 << 4,

    GRAPH_PRINT_ALL = 0xff,

    NUM_GRAPH_PRINT_FLAGS = 5,
};



// declarations



void printBoard(Board* board);
void printGraphInfo(Graph* graph, GraphPrintFlags flags);



#endif