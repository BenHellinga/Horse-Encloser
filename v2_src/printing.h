#ifndef PRINTING_H_
#define PRINTING_H_

#include <stdbool.h>

#include "graph.h"



// typedefs



typedef char (*ObjectToCharFunction)(const void* object);



// functions



void print2D(uint8_t width, uint8_t height, void** grid, ObjectToCharFunction toChar, bool border);

char nodeToChar(const void* object);
char nodeToEdgeCountChar(const void* object);

void printGraphChars(const Graph* graph);
void printGraphEdges(const Graph* graph);



#endif