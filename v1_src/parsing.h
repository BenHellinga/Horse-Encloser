#ifndef PARSING_H_
#define PARSING_H_

#include "graph.h"
#include "return.h"



// defines



#define PARSING_LINE_BUFFER_SIZE 256
#define GAMEMODE_STRING_BUFFER_SIZE 32



// functions



ReturnCode parseGraph(const char* filepath, Graph* graph);



#endif