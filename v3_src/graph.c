#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "printing.h"
#include "bfs.h"

#include "graph.h"



// declarations



static void getBaseGraphNodes(Graph* graph, Board* board);
static void fullyConnectGraph(Graph* graph, Board* board);

static bool removeUnreachableNodes(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult);
static bool mergeConsecutiveUnplaceableNodes(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult);
static bool markUnsensibleNodesUnplaceable(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult);

static void connectNodes(Node* a, Node* b);
static bool removeNodeIDFromList(NodeID id, NodeCount* numList, NodeID* list);
static bool removeNodeIDsFromList(NodeCount numRemove, NodeID* removeList, NodeCount* numList, NodeID* list);
static bool addNodeIDToList(NodeID id, NodeCount* numList, NodeID* list);
static bool isNodeIDInList(NodeID id, NodeCount numList, NodeID* list);
static NodeID getNewGroupID(GraphPrintingInfo* info);

static void mergeNodesIntoGroup(Graph* graph, GraphPrintingInfo* info,  NodeCount numNodes, NodeID* nodeIDs);
static Node* removeNodesAndGetMergedGroup(Graph* graph, NodeCount numIDs, NodeID* nodeIDs);
static void updateGraphPrintingInfoWithNewGroup(GraphPrintingInfo* info, Node* node, NodeCount numIDs, NodeID* nodeIDs);



// header functions



Graph* graphFromBoard(Board* board)
{
    // init graph
    Graph* graph = (Graph*)calloc(1, sizeof(Graph));
    getBaseGraphNodes(graph, board);
    fullyConnectGraph(graph, board);
    GraphPrintingInfo* info = getGraphPrintingInfo(graph, board);

    NodeCount numNodes = graph->numNodes;

    // init bfs
    BFSData* bfsData = initBFSData(numNodes);
    BFSArgs* bfsArgs = initBFSArgs(graph);
    BFSResult* bfsResult = initBFSResult(numNodes);

    bfsArgs->startList = (NodeID*)calloc(numNodes, sizeof(NodeID));
    bfsArgs->endMask = (EndType*)calloc(numNodes, sizeof(EndType));

    // optimize graph until no more changes can be made
    bool changed = true;
    while (changed)
    {
        printf("\n");
        printAllGraphInfo(info);

        changed = false;

        changed |= removeUnreachableNodes(graph, info, bfsData, bfsArgs, bfsResult);
        changed |= mergeConsecutiveUnplaceableNodes(graph, info, bfsData, bfsArgs, bfsResult);
        changed |= markUnsensibleNodesUnplaceable(graph, info, bfsData, bfsArgs, bfsResult);
    }

    free(bfsArgs->startList);
    free(bfsArgs->endMask);

    freeBFSData(bfsData);
    freeBFSArgs(bfsArgs);
    freeBFSResult(bfsResult);

    return graph;
}



void freeGraph(Graph* graph)
{
    for (NodeCount n = 0; n < graph->numNodes; n++)
        if (graph->nodes[n].id != NULL_ID)
            free(graph->nodes[n].edges);

    free(graph->nodes);
    free(graph->optionalEnds);
    free(graph->requiredEnds);
    free(graph);
}



// graph init



// function to populate a graph with unconnected nodes from a board
static void getBaseGraphNodes(Graph* restrict graph, Board* restrict board)
{
    uint16_t numTiles = board->numTiles;
    TileType* tiles = board->tiles;
    uint8_t width = board->width;
    uint8_t height = board->height;

    NodeCount numNodes = numTiles;
    Node* nodes = (Node*)calloc(numNodes, sizeof(Node));
    NodeCount numOptional = 0;
    NodeID* optionalEnds = (NodeID*)calloc(numNodes, sizeof(NodeID));
    NodeCount numRequired = 0;
    NodeID* requiredEnds = (NodeID*)calloc(numNodes, sizeof(NodeID));
    NodeID horse = NULL_ID;
    NodeID unicorn = NULL_ID;

    // loop over every tile in board and convert it to a node in the graph
    for (NodeCount n = 0; n < numNodes; n++)
    {
        TileType tile = tiles[n];

        Score value = TILE_INFO[tile].value;

        bool firstOrLastRow = n / width == 0 || n / width == height - 1;
        bool firstOrLastColumn = n % width == 0 || n % width == width - 1;
        bool isPortal = TILE_IS_PORTAL(tile);
        bool isNegative = value < 0;

        nodes[n] = (Node)
        {
            .id = n,
            .value = value,
            .type = TILE_INFO[tile].walkable ? (TILE_INFO[tile].placeable ? NODE_NONE : NODE_UNPLACEABLE) : NODE_UNWALKABLE,
            .numEdges = 0, // set in fullyConnectGraph()
            .edges = (NodeID*)calloc(4 + isPortal - firstOrLastRow - firstOrLastColumn, sizeof(NodeID)), // enough room for fullyConnectGraph()
        };

        if (tile == TILE_HORSE) horse = n;
        if (tile == TILE_UNICORN) unicorn = n;

        if (isNegative)
            optionalEnds[numOptional++] = n;
        else if (firstOrLastRow || firstOrLastColumn)
            requiredEnds[numRequired++] = n;
    }

    *graph = (Graph)
    {
        .numNodes = numNodes,
        .nodes = nodes,
        .numOptional = numOptional,
        .optionalEnds = optionalEnds,
        .numRequired = numRequired,
        .requiredEnds = requiredEnds,
        .horse = horse,
        .unicorn = unicorn,
    };
}



// fully connects all nodes in the graph
static void fullyConnectGraph(Graph* restrict graph, Board* restrict board)
{
    uint8_t width = board->width;
    uint8_t height = board->height;
    Node* nodes = graph->nodes;

    NodeID portals[NUM_PORTAL_PAIRS];
    for (uint8_t p = 0; p < NUM_PORTAL_PAIRS; p++)
        portals[p] = NULL_ID;

    for (uint8_t x = 0; x < width; x++)
    for (uint8_t y = 0; y < height; y++)
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



static bool removeUnreachableNodes(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult)
{
    // init bfs arguments
    if (graph->unicorn == NULL_ID)
    {
        bfsArgs->numStarts = 1;
        bfsArgs->startList[0] = graph->horse;
    }
    else
    {
        bfsArgs->numStarts = 2;
        bfsArgs->startList[0] = graph->horse;
        bfsArgs->startList[1] = graph->unicorn;
    }

    EndType* endMask = bfsArgs->endMask;
    NodeCount numNodes = graph->numNodes;
    memset(endMask, END_NONE, graph->numNodes * sizeof(EndType));
    for (NodeCount r = 0; r < graph->numRequired; r++)
    {
        NodeID required = graph->requiredEnds[r];
        bfsArgs->endMask[required] = END_REQUIRED;
    }

    bfsArgs->stopEarly = false;
    bfsArgs->includeEnds = true;

    // bfs
    BFS(bfsData, bfsArgs, bfsResult);

    BFSState* visited = bfsData->visited;
    BFSState state = bfsData->state;
    Node* nodes = graph->nodes;

    // loop over all nodes, then find and remove unreached ones
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        if (nodes[n].id == NULL_ID) continue;
        if (visited[n] == state) continue;

        Node* node = &nodes[n];
        NodeCount numEdges = node->numEdges;
        NodeID* edges = node->edges;
        NodeID id = node->id;

        for (NodeCount e = 0; e < numEdges; e++)
        {
            Node* node = &nodes[edges[e]];
            removeNodeIDFromList(id, &node->numEdges, node->edges);
        }

        free(edges);
        node->id = NULL_ID;
        info->nodes[n].status = NPS_NULL;

        changed = true;
    }

    return changed;
}



static bool mergeConsecutiveUnplaceableNodes(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult)
{
    NodeCount numNodes = graph->numNodes;
    Node* nodes = graph->nodes;

    // init bfs endmask
    EndType* endMask = bfsArgs->endMask;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];

        if (node->id == NULL_ID) continue;

        if (node->type == NODE_UNPLACEABLE)
            endMask[n] = END_NONE;
        else
            endMask[n] = END_REQUIRED;
    }

    bfsArgs->stopEarly = false;
    bfsArgs->includeEnds = false;
    bfsArgs->numStarts = 1;
    NodeID* startList = bfsArgs->startList;

    // scan over board
    bool changed = false;
    for (NodeID n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];

        // if not null or non-unplaceable
        if (node->id == NULL_ID) continue;
        if (node->type != NODE_UNPLACEABLE) continue;

        startList[0] = n;

        // do bfs to find consecutive unplaceable tiles
        BFS(bfsData, bfsArgs, bfsResult);

        // merge into group
        NodeCount numVisited = bfsData->tail;
        if (numVisited > 1)
        {
            mergeNodesIntoGroup(graph, info, numVisited, bfsData->queue);
            changed = true;
        }
    }

    return changed;
}



static bool markUnsensibleNodesUnplaceable(Graph* graph, GraphPrintingInfo* info, BFSData* bfsData, BFSArgs* bfsArgs, BFSResult* bfsResult)
{
    NodeCount numNodes = graph->numNodes;
    Node* nodes = graph->nodes;

    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        NodeID id = node->id;

        if (id == NULL_ID) continue;
        if (node->type != NODE_NONE) continue;

        if (isNodeIDInList(id, graph->numRequired, graph->requiredEnds)) continue;
        if (isNodeIDInList(id, graph->numOptional, graph->optionalEnds)) continue;

        NodeCount numEdges = node->numEdges;
        NodeID* edges = node->edges;

        if (numEdges > 2) continue;

        bool unsensible = true;
        for (NodeCount e = 0; e < numEdges; e++)
        {
            NodeID nextID = edges[e];
            Node* next = &nodes[nextID];

            if (next->type != NODE_NONE)
            {
                if (nextID == graph->horse) continue;

                if (nextID == graph->unicorn)
                {
                    unsensible = false;
                    break;
                }

                if (next->numEdges > 2)
                {
                    unsensible = false;
                    break;
                }
                
                if (isNodeIDInList(nextID, graph->numRequired, graph->requiredEnds) ||
                    isNodeIDInList(nextID, graph->numOptional, graph->optionalEnds))
                {
                    unsensible = false;
                    break;
                }
            }
        }

        if (unsensible)
        {
            node->type = NODE_UNPLACEABLE;
            changed = true;
        }
    }

    return changed;
}



// helpers



static void connectNodes(Node* a, Node* b)
{
    a->edges[a->numEdges++] = b->id;
    b->edges[b->numEdges++] = a->id;
}



static bool removeNodeIDFromList(NodeID id, NodeCount* numList, NodeID* list)
{
    NodeCount num = *numList;

    for (NodeCount i = 0; i < num; i++)
    {
        if (list[i] == id)
        {
            // shift everything after it down by one
            for (NodeCount j = i; j < num - 1; j++)
                list[j] = list[j + 1];

            (*numList)--;
            return true;
        }
    }
    return false;
}



static bool removeNodeIDsFromList(NodeCount numRemove, NodeID* removeList, NodeCount* numList, NodeID* list)
{
    bool anyRemoved = false;
    NodeCount write = 0;

    for (NodeCount read = 0; read < *numList; read++)
    {
        bool shouldRemove = false;
        for (NodeCount i = 0; i < numRemove; i++)
        {
            if (list[read] == removeList[i])
            {
                shouldRemove = true;
                break;
            }
        }

        if (shouldRemove)
            anyRemoved = true;
        else
            list[write++] = list[read];
    }

    *numList = write;
    return anyRemoved;
}



static bool addNodeIDToList(NodeID id, NodeCount* numList, NodeID* list)
{
    NodeCount num = *numList;
    for (NodeCount i = 0; i < num; i++)
        if (list[i] == id)
            return false;

    list[num] = id;
    (*numList)++;
    return true;
}



static bool isNodeIDInList(NodeID id, NodeCount numList, NodeID* list)
{
    for (NodeCount i = 0; i < numList; i++)
        if (list[i] == id)
            return true;
    return false;
}



static NodeID getNewGroupID(GraphPrintingInfo* info)
{
    NodeCount numGroups = info->numGroups;
    NodeGroup* groups = info->groups;

    for (NodeCount g = 0; g < numGroups; g++)
        if (groups[g].id == NULL_ID)
            return g;

    (info->numGroups)++;
    return numGroups;
}



static void mergeNodesIntoGroup(Graph* graph, GraphPrintingInfo* info, NodeCount numIDs, NodeID* nodeIDs)
{
    Node* newNode = removeNodesAndGetMergedGroup(graph, numIDs, nodeIDs);
    NodeID newNodeID = newNode->id;
    Score newValue = newNode->value;
    NodeCount newNumEdges = newNode->numEdges;
    NodeID* newEdges = newNode->edges;

    Node* nodes = graph->nodes;

    // connect grouped node to edges
    for (NodeCount e = 0; e < newNumEdges; e++)
    {
        Node* next = &nodes[newEdges[e]];
        next->edges[next->numEdges++] = newNodeID;
    }

    // update horse and unicorn positions
    NodeID horse = graph->horse;
    if (horse != NULL_ID)
    {
        for (NodeCount n = 0; n < numIDs; n++)
        {
            if (nodeIDs[n] == horse)
            {
                graph->horse = newNodeID;
                break;
            }
        }
    }
    NodeID unicorn = graph->unicorn;
    if (unicorn != NULL_ID)
    {
        for (NodeCount n = 0; n < numIDs; n++)
        {
            if (nodeIDs[n] == unicorn)
            {
                graph->unicorn = newNodeID;
                break;
            }
        }
    }

    // update ends
    
    bool requiredRemoved = removeNodeIDsFromList(numIDs, nodeIDs, &graph->numRequired, graph->requiredEnds);
    removeNodeIDsFromList(numIDs, nodeIDs, &graph->numOptional, graph->optionalEnds);

    if (requiredRemoved)
        addNodeIDToList(newNodeID, &graph->numRequired, graph->requiredEnds);
    else if (newValue < 0)
        addNodeIDToList(newNodeID, &graph->numOptional, graph->optionalEnds);

    // update printing info
    updateGraphPrintingInfoWithNewGroup(info, newNode, numIDs, nodeIDs);
}



static Node* removeNodesAndGetMergedGroup(Graph* graph, NodeCount numIDs, NodeID* nodeIDs)
{
    Node* nodes = graph->nodes;

    NodeID newNodeID = nodeIDs[0];
    Node* newNode = &nodes[newNodeID];
    NodeCount newNumEdges = 0;
    NodeID* newEdges = (NodeID*)calloc(numIDs * 3 + 2, sizeof(NodeID));
    Score newValue = 0;

    // label all merged nodes as null
    for (NodeCount n = 0; n < numIDs; n++)
        nodes[nodeIDs[n]].id = NULL_ID;

    // loop over all nodes in group
    for (NodeCount n = 0; n < numIDs; n++)
    {
        NodeID id = nodeIDs[n];
        Node* node = &nodes[id];

        newValue += node->value;

        NodeCount numEdges = node->numEdges;
        NodeID* edges = node->edges;

        // get all edges that are non null (not part of group)
        for (NodeCount e = 0; e < numEdges; e++)
        {
            Node* next = &nodes[edges[e]];
            NodeID nextID = next->id;

            if (nextID == NULL_ID) continue;

            addNodeIDToList(nextID, &newNumEdges, newEdges);
            removeNodeIDFromList(id, &next->numEdges, next->edges);
        }
        free(edges);
    }
    newEdges = (NodeID*)realloc(newEdges, newNumEdges * sizeof(NodeID));

    // make new grouped node
    *newNode = (Node)
    {
        .id = newNodeID,
        .value = newValue,
        .type = NODE_UNPLACEABLE,
        .numEdges = newNumEdges,
        .edges = newEdges,
    };

    return newNode;
}



static void updateGraphPrintingInfoWithNewGroup(GraphPrintingInfo* info, Node* node, NodeCount numIDs, NodeID* nodeIDs)
{
    NodePrintingInfo* nodeInfos = info->nodes;

    // count how many merges nodes in the group (including previously merged groups)
    NodeCount newNumGroupedIDs = 0;
    for (NodeCount n = 0; n < numIDs; n++)
    {
        NodePrintingInfo* nodeInfo = &nodeInfos[nodeIDs[n]];

        if (nodeInfo->status == NPS_NODE)
            newNumGroupedIDs++;
        else
            newNumGroupedIDs += nodeInfo->group->numGroupedIDs;
    }

    NodeID* newGroupedIDs = (NodeID*)calloc(newNumGroupedIDs, sizeof(NodeID));
    newNumGroupedIDs = 0;

    // load group ids into new array
    for (NodeCount n = 0; n < numIDs; n++)
    {
        NodeID id = nodeIDs[n];
        NodePrintingInfo* nodeInfo = &nodeInfos[id];

        if (nodeInfo->status == NPS_NODE)
        {
            newGroupedIDs[newNumGroupedIDs++] = id;
            continue;
        }

        NodeGroup* group = nodeInfo->group;
        NodeCount numGroupedIDs = group->numGroupedIDs;
        NodeID* groupedIDs = group->groupedIDs;

        memcpy(newGroupedIDs + newNumGroupedIDs, groupedIDs, numGroupedIDs * sizeof(NodeID));
        newNumGroupedIDs += numGroupedIDs;
        
        group->id = NULL_ID;
        free(groupedIDs);
    }

    NodeID groupID = getNewGroupID(info);
    NodeGroup* newGroup = &info->groups[groupID];
    *newGroup = (NodeGroup)
    {
        .id = groupID,
        .node = node,
        .numGroupedIDs = newNumGroupedIDs,
        .groupedIDs = newGroupedIDs,
    };

    // update nodePrintingInfos
    for (NodeCount n = 0; n < newNumGroupedIDs; n++)
    {
        NodePrintingInfo* nodeInfo = &nodeInfos[newGroupedIDs[n]];
        nodeInfo->status = NPS_GROUP;
        nodeInfo->group = newGroup;
    }

    // disable inner node visibility
    uint8_t width = info->width;
    NodeCount numNodes = info->numNodes;
    for (NodeCount n = 0; n < newNumGroupedIDs; n++)
    {
        NodeID id = newGroupedIDs[n];

        bool innerNode = true;
        for (uint8_t i = 0; i < 4; i++)
        {
            NodeID nid;
            switch (i)
            {
                case 0: nid = id - 1; break;
                case 1: nid = id + 1; break;
                case 2: nid = id - width; break;
                case 3: nid = id + width; break;
            }
            if (nid < 0 || nid >= numNodes) continue;
            
            NodePrintingInfo* nodeInfo = &nodeInfos[nid];

            if (nodeInfo->status == NPS_NODE || (nodeInfo->status == NPS_GROUP && nodeInfo->group->id != groupID))
                innerNode = false;
        }

        if (innerNode)
            nodeInfos[id].printGroupLabel = false;
    }
}