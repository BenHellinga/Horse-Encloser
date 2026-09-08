#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bfs.h"



// declarations



static BFSState getNextBFSState();
static void runBFS();



// the one and only context never directly accessed outside this file
static BFSContext bfs;



// init / free



BFSContext* initBFSContext(NodeCount numNodes)
{
    bfs.data.state = 0;
    bfs.data.numNodes = numNodes;
    bfs.data.visited = calloc(numNodes, sizeof(BFSState));
    bfs.data.queue = calloc(numNodes, sizeof(NodeID)); // each node is enqueued at most once
    bfs.data.paths = calloc(numNodes, sizeof(NodeID));

    return &bfs;
}



void freeBFSContext()
{
    free(bfs.data.visited);
    free(bfs.data.queue);
    free(bfs.data.paths);
}



// run setup



void setBFSArgs(Graph* graph, NodeFlags blockFlags, NodeFlags endFlags, bool stopEarly)
{
    bfs.args.graph = graph;
    bfs.args.blockFlags = blockFlags;
    bfs.args.endFlags = endFlags;
    bfs.args.stopEarly = stopEarly;
}



void multiBFS(NodeID* startList, NodeCount numStarts)
{
    BFSState state = getNextBFSState();

    Graph* restrict graph = bfs.args.graph;
    Node* restrict nodes = graph->nodes;
    NodeFlags endFlags = bfs.args.endFlags;
    bool stopEarly = bfs.args.stopEarly;

    bfs.data.head = 0;
    bfs.data.tail = 0;
    bfs.data.endReached = false;
    bfs.data.endID = NULL_ID;
    bfs.data.value = 0;

    for (NodeCount i = 0; i < numStarts; i++)
    {
        NodeID start = startList[i];

        bfs.data.visited[start] = state;
        bfs.data.paths[start] = NULL_ID;

        if (nodes[start].flags & endFlags)
        {
            bfs.data.endReached = true;

            if (!stopEarly) continue;

            bfs.data.endID = start;
            return;
        }

        bfs.data.queue[bfs.data.tail++] = start;
    }

    runBFS();
}



void singleBFS(NodeID start)
{
    multiBFS(&start, 1);
}



// general bfs



static void runBFS()
{
    Graph* restrict graph = bfs.args.graph;
    Node* restrict nodes = graph->nodes;

    BFSState state = bfs.data.state;
    NodeFlags blockFlags = bfs.args.blockFlags;
    NodeFlags endFlags = bfs.args.endFlags;

    bfs.data.value = 0;

    while (bfs.data.head < bfs.data.tail)
    {
        NodeID current = bfs.data.queue[bfs.data.head++];
        Node* restrict node = &nodes[current];

        bfs.data.value += node->value;

        NodeID* restrict edges = node->edges.list;
        NodeCount numEdges = node->edges.num;

        for (NodeCount e = 0; e < numEdges; e++)
        {
            NodeID next = edges[e];
            Node* restrict nextNode = &nodes[next];

            if (bfs.data.visited[next] == state) continue;
            if (nextNode->flags & blockFlags) continue;

            bfs.data.visited[next] = state;
            bfs.data.paths[next] = current;

            if (nextNode->flags & endFlags)
            {
                bfs.data.endReached = true;

                if (!bfs.args.stopEarly) continue;

                bfs.data.endID = next;
                return;
            }

            bfs.data.queue[bfs.data.tail++] = next;
        }
    }
}



bool solverBFS()
{
    Graph* restrict graph = bfs.args.graph;
    Node* restrict nodes = graph->nodes;

    BFSState state = bfs.data.state;
    Score value = 0;
    bool endReached = false;
    NodeID endID = NULL_ID;

    while (bfs.data.head < bfs.data.tail)
    {
        NodeID current = bfs.data.queue[bfs.data.head++];
        Node* restrict node = &nodes[current];

        value += node->value;

        NodeID* restrict edges = node->edges.list;
        NodeCount numEdges = node->edges.num;

        for (NodeCount e = 0; e < numEdges; e++)
        {
            NodeID next = edges[e];
            Node* restrict nextNode = &nodes[next];

            if (bfs.data.visited[next] == state) continue;
            if (nextNode->flags & FLAG_INSIDE) continue;

            if (nextNode->flags & (FLAG_REQUIRED | FLAG_OUTSIDE))
            {
                bfs.data.visited[next] = state;
                bfs.data.paths[next] = current;

                endReached = true;
                endID = next;
                goto stop;
            }

            bfs.data.visited[next] = state;
            bfs.data.queue[bfs.data.tail++] = next;
            bfs.data.paths[next] = current;
        }
    }

    stop:

    bfs.data.endReached = endReached;
    bfs.data.endID = endID;
    bfs.data.value = value;

    return endReached;
}



// helpers



static BFSState getNextBFSState()
{
    if (bfs.data.state == MAX_BFS_STATE)
    {
        bfs.data.state = 0;
        memset(bfs.data.visited, 0, sizeof(BFSState) * bfs.data.numNodes);
    }

    return ++bfs.data.state;
}



void recoverPath(NodeID* path, NodeID* start)
{
    NodeID current = bfs.data.endID;
    path[current] = NULL_ID;

    NodeID prev = bfs.data.paths[current];
    while (prev != NULL_ID)
    {
        path[prev] = current;
        current = prev;
        prev = bfs.data.paths[prev];
    }

    *start = current;
}



void recoverReversePath(NodeID* path, NodeID* start)
{
    NodeID current = bfs.data.endID;

    while (current != NULL_ID)
    {
        NodeID next = bfs.data.paths[current];
        path[current] = next;
        current = next;
    }

    *start = bfs.data.endID;
}