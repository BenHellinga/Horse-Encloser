#include <stdlib.h>
#include <stdio.h>

#include "printing.h"
#include "bfs.h"

#include "graph.h"



// declarations



static void getBaseGraphNodes(Graph* graph, Board* board);
static void fullyConnectGraph(Graph* graph, Board* board);

static void removeUnreachableNodes(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult);

static void connectNodes(Node* a, Node* b);
static void removeEdgeID(Node* n, NodeID id);



// header functions



Graph* graphFromBoard(Board* board)
{
    // init graph
    Graph* graph = (Graph*)calloc(1, sizeof(Graph));
    getBaseGraphNodes(graph, board);
    fullyConnectGraph(graph, board);
    GraphPrintingInfo* info = getGraphPrintingInfo(graph, board);

    // init bfs
    BFSData* bfsData = initBFSData(graph->numNodes);
    BFSArgs* bfsArgs = initBFSArgs(graph);
    BFSResult* bfsResult = initBFSResult(graph->numNodes);

    if (board->gamemode == GAMEMODE_CLASSIC || board->gamemode == GAMEMODE_COSTLY)
    {
        bfsArgs->numStarts = 1;
        bfsArgs->starts[0] = graph->horse;
    }
    else
    {
        bfsArgs->numStarts = 2;
        bfsArgs->starts[0] = graph->horse;
        bfsArgs->starts[1] = graph->unicorn;
    }

    // optimize graph until no more changes can be made
    bool changed = true;
    bool endsChanged = true;
    while (changed)
    {
        printf("\n\n");
        printGraphTypes(info);
        printGraphEdges(info);

        if (endsChanged)
        {
            removeUnreachableNodes(graph, info, bfsData, bfsArgs, bfsResult);
            endsChanged = false;
        }

        changed = endsChanged;
    }

    printf("\n\n");
    printGraphTypes(info);
    printGraphEdges(info);

    freeBFSData(bfsData);
    freeBFSArgs(bfsArgs);
    freeBFSResult(bfsResult);
    return graph;
}



void freeGraph(Graph* graph)
{
    Node* nodes = graph->nodes;

    for (NodeID n = 0; n < graph->numNodes; ++n)
    {
        if (nodes[n].id == NULL_ID) continue;
        free(nodes[n].edges);
    }
    
    free(graph->nodes);
    free(graph);
}



// graph init



// function to populate a graph with unconnected nodes from a board
static void getBaseGraphNodes(Graph* graph, Board* board)
{
    uint16_t numTiles = board->numTiles;
    TileType* tiles = board->tiles;
    uint8_t width = board->width;
    uint8_t height = board->height;

    NodeCount numNodes = numTiles;
    Node* nodes = (Node*)calloc(numNodes, sizeof(Node));
    NodeID horse = NULL_ID;
    NodeID unicorn = NULL_ID;
    NodeCount numOptional = -1;
    NodeID* optionalEnds = (NodeID*)calloc(numNodes, sizeof(NodeID));
    NodeCount numRequired = -1;
    NodeID* requiredEnds = (NodeID*)calloc(numNodes, sizeof(NodeID));

    // loop over every tile in board and convert it to a node in the graph
    for (NodeID n = 0; n < numNodes; ++n)
    {
        TileType tile = tiles[n];

        ScoreValue value = TILE_INFO[tile].value;

        bool firstOrLastRow = n / width == 0 || n / width == height - 1;
        bool firstOrLastColumn = n % width == 0 || n % width == width - 1;
        bool isPortal = TILE_IS_PORTAL(tile);
        bool isNegative = value < 0;

        nodes[n] = (Node){
            .id = n,
            .value = value,
            .type = TILE_INFO[tile].walkable ? (TILE_INFO[tile].placeable ? NODE_NONE : NODE_UNPLACEABLE) : NODE_UNWALKABLE,
            .numEdges = 0, // set in fullyConnectGraph()
            .edges = (NodeID*)calloc(4 + isPortal - firstOrLastRow - firstOrLastColumn, sizeof(NodeID)), // enough room for fullyConnectGraph()
        };

        if (tile == TILE_HORSE) horse = n;
        if (tile == TILE_UNICORN) unicorn = n;

        if (isNegative)
            optionalEnds[++numOptional] = n;
        else if (firstOrLastRow || firstOrLastColumn)
            requiredEnds[++numRequired] = n;
    }

    *graph = (Graph){
        .numNodes = numNodes,
        .nodes = nodes,
        .horse = horse,
        .unicorn = unicorn,
        .numOptional = ++numOptional,
        .optionalEnds = optionalEnds,
        .numRequired = ++numRequired,
        .requiredEnds = requiredEnds
    };
}



// fully connects all nodes in the graph
static void fullyConnectGraph(Graph* graph, Board* board)
{
    uint8_t width = board->width;
    uint8_t height = board->height;
    Node* nodes = graph->nodes;

    NodeID portals[NUM_PORTAL_PAIRS];
    for (uint8_t p = 0; p < NUM_PORTAL_PAIRS; ++p)
        portals[p] = NULL_ID;

    for (uint8_t x = 0; x < width; ++x)
    for (uint8_t y = 0; y < height; ++y)
    {
        NodeID id = x + y * width;
        Node* node = &nodes[id];
        TileType tile = board->tiles[id];

        if (x != 0) connectNodes(node, &nodes[id - 1]);
        if (y != 0) connectNodes(node, &nodes[id - width]);

        if (TILE_IS_PORTAL(tile))
        {
            uint8_t pair = PORTAL_PAIR(tile);

            if (portals[pair] == NULL_ID)
                portals[pair] = id;
            else
                connectNodes(node, &nodes[portals[pair]]);
        }
    }
}



// graph optimizers



static void removeUnreachableNodes(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult)
{
    BFS(bfsData, bfsArgs, bfsResult);

    BFSState* visited = bfsData->visited;
    BFSState state = bfsData->state;
    Node* nodes = graph->nodes;

    NodeCount numNodes = graph->numNodes;
    for (NodeID n = 0; n < numNodes; ++n)
    {
        if (visited[n] == state) continue;

        Node* node = &nodes[n];
        NodeCount numEdges = node->numEdges;
        NodeID* edges = node->edges;
        NodeID id = node->id;

        for (NodeID e = 0; e < numEdges; ++e)
            removeEdgeID(&nodes[edges[e]], id);

        free(edges);
        node->id = NULL_ID;
        info->nodes[n] = NULL;
    }
}



// helpers



static void connectNodes(Node* a, Node* b)
{
    a->edges[a->numEdges++] = b->id;
    b->edges[b->numEdges++] = a->id;
}



static void removeEdgeID(Node* n, NodeID id)
{
    NodeCount numEdges = n->numEdges;
    NodeID* edges = n->edges;
    NodeID e = 0;

    while (e < numEdges)
    {
        if (edges[e++] != id)
            continue;

        while (e < numEdges)
        {
            edges[e - 1] = edges[e];
            ++e;
        }

        --n->numEdges;
        break;
    }
}