#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bfs.h"



// defines



// declarations



static BFSState getNextBFSState(BFSData* bfsData);
static void recoverBFSPath(NodeID* restrict paths, NodeID endID, NodeCount numNodes, NodeID* restrict path, NodeCount* restrict pathLength);



// init / free



BFSData* initBFSData(NodeCount numNodes)
{
    BFSData* bfsData = (BFSData*)calloc(1, sizeof(BFSData));

    bfsData->state = 0;
    bfsData->numNodes = numNodes;
    bfsData->visited = (BFSState*)calloc(numNodes, sizeof(BFSState));
    bfsData->queue = (NodeID*)calloc(numNodes, sizeof(NodeID));
    bfsData->paths = (NodeID*)calloc(numNodes, sizeof(NodeID));

    return bfsData;
}



void freeBFSData(BFSData* bfsData)
{
    free(bfsData->visited);
    free(bfsData->queue);
    free(bfsData->paths);
    free(bfsData);
}



BFSArgs* initBFSArgs(Graph* graph)
{
    BFSArgs* bfsArgs = malloc(sizeof(BFSArgs));

    bfsArgs->graph = graph;
    bfsArgs->numStarts = 0;
    bfsArgs->startList = NULL;
    bfsArgs->endMask = NULL;
    bfsArgs->stopEarly = true;
    bfsArgs->includeEnds = true;

    return bfsArgs;
}



void freeBFSArgs(BFSArgs* bfsArgs)
{
    free(bfsArgs);
}



BFSResult* initBFSResult(NodeCount numNodes)
{
    BFSResult* bfsResult = malloc(sizeof(BFSResult));

    bfsResult->endReached = false;
    bfsResult->score = 0;
    bfsResult->endID = NULL_ID;
    bfsResult->pathLength = 0;
    bfsResult->path = (NodeID*)calloc(numNodes, sizeof(NodeID));

    return bfsResult;
}



void freeBFSResult(BFSResult* bfsResult)
{
    free(bfsResult->path);
    free(bfsResult);
}



// bfs



void BFS(BFSData* restrict bfsData, BFSArgs* restrict bfsArgs, BFSResult* restrict bfsResult)
{
    // cache everything for hot loop
    Graph* restrict graph = bfsArgs->graph;
    Node* restrict nodes = graph->nodes;
    NodeID* restrict startList = bfsArgs->startList;
    EndType* restrict endMask = bfsArgs->endMask;

    BFSState* restrict visited = bfsData->visited;
    NodeID* restrict queue = bfsData->queue;
    NodeID* restrict paths = bfsData->paths;

    // invalidate old data
    BFSState state = getNextBFSState(bfsData);
    bool stopEarly = bfsArgs->stopEarly;
    bool excludeEnds = !bfsArgs->includeEnds;

    NodeCount head = 0;
    NodeCount tail = 0;

    // add startList to visited/queue/paths
    NodeCount numStarts = bfsArgs->numStarts;
    for (NodeCount i = 0; i < numStarts; i++)
    {
        NodeID start = startList[i];

        visited[start] = state;
        queue[tail++] = start;
        paths[start] = NULL_ID;
    }

    Score score = 0;
    bool endReached = false;
    NodeID endID = NULL_ID;

    // explore nodes until queue is empty or end is found
    while (head < tail)
    {
        NodeID current = queue[head++];
        Node* restrict node = &nodes[current];

        score += node->value;

        NodeID* restrict edges = node->edges;
        NodeCount numEdges = node->numEdges;

        // loop over edges
        for (NodeCount e = 0; e < numEdges; e++)
        {
            NodeID next = edges[e];

            if (visited[next] == state) continue; // already visited
            if (nodes[next].type == NODE_UNWALKABLE) continue; // cannot pass through

            // check if next is an end node
            if (endMask[next] != END_NONE) // IF START/END ARE THE SAME NODE, THIS WONT GET IT
            {
                if (stopEarly)
                {
                    visited[next] = state;
                    queue[tail++] = next;
                    paths[next] = current;

                    endReached = true;
                    endID = next; // the end tile itself, so the path includes it
                    goto stop;
                }
                
                if (excludeEnds) continue;

                visited[next] = state;
                paths[next] = current;
                continue;
            }

            visited[next] = state;
            queue[tail++] = next;
            paths[next] = current;
        }
    }

    stop:

    bfsData->head = head;
    bfsData->tail = tail;

    bfsResult->endReached = endReached;

    if (endReached)
    {
        bfsResult->endID = endID;
        recoverBFSPath(paths, endID, bfsData->numNodes, bfsResult->path, &bfsResult->pathLength);
    }
    else
    {
        bfsResult->score = score;
    }
}



// bfs helpers



static BFSState getNextBFSState(BFSData* bfsData)
{
    if (bfsData->state == MAX_BFS_STATE)
    {
        bfsData->state = 0;
        memset(bfsData->visited, 0, sizeof(BFSState) * bfsData->numNodes);
    }

    return ++bfsData->state;
}



static void recoverBFSPath(NodeID* restrict paths, NodeID endID, NodeCount numNodes, NodeID* restrict path, NodeCount* restrict pathLength)
{
    NodeID current = endID;
    NodeCount count = numNodes;

    while (current != NULL_ID)
    {
        path[--count] = current;
        current = paths[current];
    }

    *pathLength = numNodes - count - 1;
}