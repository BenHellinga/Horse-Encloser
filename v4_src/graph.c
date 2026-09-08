#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "graph.h"
#include "printing.h"



// defines



// declarations



static void getBaseGraphNodes(Graph* graph, Board* board);
static void fullyConnectGraph(Graph* graph, Board* board);
static bool removeUnreachableNodes(Graph* graph, BFSContext* bfs);
static bool mergeConsecutiveOccupiedNodes(Graph* graph, BFSContext* bfs);
static bool markUnsensibleNodesOccupied(Graph* graph);
static bool pushForwardRequiredEnds(Graph* graph);
static bool disconnectConsecutiveRequired(Graph* graph);
static bool mergeSimilarOccupiedNodes(Graph* graph);
static bool mergeDeadEnds(Graph* graph, BFSContext* bfs);

static void connectNodes(Node* a, Node* b);
static void disconnectNodes(Node* a, Node* b);
static bool removeIDFromNodeList(NodeList* list, NodeID id);
static bool addIDToNodeList(NodeList* list, NodeID id);
static bool nodeListsAreTheSame(NodeList *listA, NodeList *listB);

static void mergeNodeListIntoGroup(Graph* graph, NodeList* group);
static void removeInnerGroupLabelTiles(Graph* graph, NodeList* labelTiles);
static NodeID getNewGroupID(Graph* graph);

static void defragGraph(Graph* graph);



// header functions



Graph* graphFromBoard(Board* board)
{
    // init graph
    Graph* graph = (Graph*)calloc(1, sizeof(Graph));
    getBaseGraphNodes(graph, board);
    fullyConnectGraph(graph, board);

    BFSContext* bfsContext = initBFSContext(graph->numNodes);

    bool changed = true;
    while (changed)
    {
        changed = false;

        changed |= removeUnreachableNodes(graph, bfsContext);
        changed |= mergeConsecutiveOccupiedNodes(graph, bfsContext);
        changed |= markUnsensibleNodesOccupied(graph);
        changed |= pushForwardRequiredEnds(graph);
        changed |= disconnectConsecutiveRequired(graph);
        changed |= mergeSimilarOccupiedNodes(graph);
        changed |= mergeDeadEnds(graph, bfsContext);
    }

    defragGraph(graph);
    freeBFSContext();
    return graph;
}



void freeGraph(Graph* graph)
{
    free(graph->buffer);
    free(graph);
}



// graph init



// function to populate a graph with unconnected nodes from a board
static void getBaseGraphNodes(Graph* graph, Board* board)
{
    Board b = *board;

    // calloc main buffers
    NodeCount numNodes = b.width * b.height;
    Node* nodes = (Node*)calloc(numNodes, sizeof(Node));

    GraphTile* tiles = (GraphTile*)calloc(numNodes, sizeof(GraphTile));
    TileGroup* groups = (TileGroup*)calloc(numNodes / 2, sizeof(TileGroup));

    NodeID horse = NULL_ID;
    NodeID unicorn = NULL_ID;

    // init main buffers from board
    for (NodeCount n = 0; n < numNodes; n++)
    {
        TileType tile = b.tiles[n];
        TileInfo info = TILE_INFO[tile];

        bool firstOrLastRow = n / b.width == 0 || n / b.width == b.height - 1;
        bool firstOrLastColumn = n % b.width == 0 || n % b.width == b.width - 1;
        bool isPortal = TILE_IS_PORTAL(tile);

        // get flag values
        bool isSolid = tile == TILE_WALL || tile == TILE_WATER;
        bool isOccupied = tile != TILE_EMPTY && !isSolid;
        bool isRequired = firstOrLastColumn || firstOrLastRow;
        bool isOptional = info.value < 0;

        // combine flags into NodeFlag
        NodeFlags flags = (isSolid    ? FLAG_SOLID    : 0) |
                          (isOccupied ? FLAG_OCCUPIED : 0) |
                          (isRequired ? FLAG_REQUIRED : 0) |
                          (isOptional ? FLAG_OPTIONAL : 0);

        NodeCount numEdges = 4 + isPortal - firstOrLastRow - firstOrLastColumn;
        NodeID* edgeList = (NodeID*)calloc(numEdges, sizeof(NodeID));

        // set node values
        nodes[n] = (Node)
        {
            .nodeID = n,
            .tileID = n,
            .value = info.value,
            .flags = flags,
            .edges.num = 0, // set in fullyConnectGraph();
            .edges.list = edgeList, // set in fullyConnectGraph();
        };

        // set graphtile values
        tiles[n] = (GraphTile)
        {
            .type = GTT_NODE,
            .nodeID = n,
            .group = NULL,
            .noPrint = false,
        };

        // record horse/unicorn locations
        if (tile == TILE_HORSE) horse = n;
        if (tile == TILE_UNICORN) unicorn = n;
    }

    // set graph values
    *graph = (Graph)
    {
        .numNodes = numNodes,
        .nodes = nodes,
        .numEdges = 0, // set in defragGraph()
        .edges = NULL, // set in defragGraph()

        .horse = horse,
        .unicorn = unicorn,

        .width = b.width,
        .height = b.height,
        .numWalls = b.numWalls,
        .gamemode = b.gamemode,
        .tiles = tiles,
        .numGroups = 0,
        .groups = groups,
    };
}



// fully connects all nodes in the graph
static void fullyConnectGraph(Graph* graph, Board* board)
{
    // init portal pairs array
    NodeID portals[NUM_PORTAL_PAIRS];
    for (uint8_t p = 0; p < NUM_PORTAL_PAIRS; p++)
        portals[p] = NULL_ID;

    // cache required info
    uint8_t width = board->width;
    uint8_t height = board->height;
    TileType* tiles = board->tiles;
    Node* nodes = graph->nodes;

    // loop over all tiles
    for (uint8_t x = 0; x < width; x++)
    for (uint8_t y = 0; y < height; y++)
    {
        NodeID id = x + y * width;
        Node* node = &nodes[id];
        TileType tile = tiles[id];

        // connect to adjacent tiles
        if (x != 0) connectNodes(node, &nodes[id - 1]);
        if (y != 0) connectNodes(node, &nodes[id - width]);

        // connect through portal connections
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



static bool removeUnreachableNodes(Graph* graph, BFSContext* bfs)
{
    NodeID startList[2];
    NodeCount numStarts = 0;

    // start at the horse/unicorn
    if (graph->horse != NULL_ID) startList[numStarts++] = graph->horse;
    if (graph->unicorn != NULL_ID) startList[numStarts++] = graph->unicorn;

    // explore all reachable
    setBFSArgs(graph, FLAG_SOLID, FLAG_REQUIRED, false);
    multiBFS(startList, numStarts);

    // cache info
    BFSState* visited = bfs->data.visited;
    BFSState state = bfs->data.state;
    Node* nodes = graph->nodes;
    NodeCount numNodes = graph->numNodes;
    GraphTile* tiles = graph->tiles;
    
    // scan through all nodes
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        NodeID id = node->nodeID;

        if (id == NULL_ID) continue;
        if (visited[n] == state) continue;

        // node is non-null and not reachable

        // remove self from all edges
        NodeList edges = node->edges;
        for (NodeCount e = 0; e < edges.num; e++)
        {
            Node* neighbor = &nodes[edges.list[e]];
            removeIDFromNodeList(&neighbor->edges, id);
        }

        // remove node from graph
        free(edges.list);
        node->nodeID = NULL_ID;

        GraphTile* tile = &tiles[node->tileID];

        // nullify group and labelTiles
        if (tile->type == GTT_GROUP)
        {
            NodeList labelTiles = tile->group->labelTiles;
            tile->group->groupID = NULL_ID;

            for (NodeCount t = 0; t < labelTiles.num; t++)
                tiles[labelTiles.list[t]].type = GTT_NULL;
            free(labelTiles.list);
        }
        else tile->type = GTT_NULL;

        changed = true;
    }

    return changed;
}



// merges runs of consecutive occupied nodes, since once you're on one you're on them all
static bool mergeConsecutiveOccupiedNodes(Graph* graph, BFSContext* bfs)
{
    Node* nodes = graph->nodes;
    NodeCount numNodes = graph->numNodes;

    // scratch flag, mark every non occupied node as inside so bfs cant leave the occupied cluster
    for (NodeCount n = 0; n < numNodes; n++)
    {
        if (nodes[n].nodeID == NULL_ID) continue;

        if (nodes[n].flags & FLAG_OCCUPIED)
            nodes[n].flags &= ~FLAG_INSIDE;
        else
            nodes[n].flags |= FLAG_INSIDE;
    }

    setBFSArgs(graph, FLAG_INSIDE, 0, false); // block on inside, no end flag, fully explore the cluster

    bool changed = false;

    // iterate over all nodes
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];

        if (node->nodeID == NULL_ID) continue;
        if (!(node->flags & FLAG_OCCUPIED)) continue;

        // node is non-null and occupied

        singleBFS(node->nodeID);

        // wrap the bfs queue as a nodelist, queue holds every visited node up to tail
        NodeList group =
        {
            .num = bfs->data.head,
            .list = bfs->data.queue,
        };

        if (group.num > 1)
        {
            mergeNodeListIntoGroup(graph, &group);
            changed = true;
        }
    }

    return changed;
}



// marks nodes where it makes no sense to place a wall as occupied
static bool markUnsensibleNodesOccupied(Graph* graph)
{
    Node* nodes = graph->nodes;
    NodeCount numNodes = graph->numNodes;

    // loop over all nodes
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        NodeID id = node->nodeID;

        if (id == NULL_ID) continue;
        if (node->flags & (FLAG_SOLID | FLAG_OCCUPIED)) continue; // only plain empty nodes are candidates
        if (node->flags & (FLAG_REQUIRED | FLAG_OPTIONAL)) continue; // ends are always sensible

        // node is non-null, empty, and not an end

        NodeList edges = node->edges;
        if (edges.num > 2) continue; // junctions are always sensible as is

        // check all edges for something that makes it a sensible wall location
        bool unsensible = true;
        for (NodeCount e = 0; e < edges.num; e++)
        {
            NodeID nextID = edges.list[e];
            Node* next = &nodes[nextID];

            if (!(next->flags & (FLAG_SOLID | FLAG_OCCUPIED))) continue; // skip if plain, empty neighbor
            if (nextID == graph->horse) continue; // next to horse is fine

            // we may want to block the unicorn on quarrel, so a tile beside it is a sensible location
            if (nextID == graph->unicorn)
            {
                if (graph->gamemode == GAMEMODE_QUARREL)
                {
                    unsensible = false;
                    break;
                }
                continue;
            }

            // if the next tile is occupied and a junction, we may want to block before the junction
            if (next->edges.num > 2)
            {
                unsensible = false;
                break;
            }

            // if the next tile is occupied and an end, we may want to block before the end
            if (next->flags & (FLAG_REQUIRED | FLAG_OPTIONAL))
            {
                unsensible = false;
                break;
            }
        }

        if (unsensible)
        {
            node->flags |= FLAG_OCCUPIED;
            changed = true;
        }
    }

    return changed;
}



static bool pushForwardRequiredEnds(Graph* graph)
{
    NodeCount numNodes = graph->numNodes;
    Node* nodes = graph->nodes;

    // loop over all nodes
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        if (node->nodeID == NULL_ID) continue;
        
        NodeFlags flags = node->flags;

        if (!(flags & FLAG_REQUIRED)) continue;
        if (!(flags & FLAG_OCCUPIED)) continue;

        // node is non-null, required, and occupied

        // push required flag forward, as the occupied tile itself cant be walled
        NodeList edges = node->edges;
        for (NodeCount e = 0; e < edges.num; e++)
            nodes[edges.list[e]].flags |= FLAG_REQUIRED;

        changed = true;
    }

    return changed;
}



static bool disconnectConsecutiveRequired(Graph* graph)
{
    NodeCount numNodes = graph->numNodes;
    Node* nodes = graph->nodes;

    // loop over all nodes
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        if (node->nodeID == NULL_ID) continue;
        
        NodeFlags flags = node->flags;
        if (!(flags & FLAG_REQUIRED)) continue;

        // node is non-null and required

        // loop over all edges
        NodeList edges = node->edges;
        for (NodeCount e = 0; e < edges.num; e++)
        {
            Node* next = &nodes[edges.list[e]];
            if (!(next->flags & FLAG_REQUIRED)) continue;

            // neighbor is also required, disconnect them

            disconnectNodes(node, next);
            changed = true;
        }
    }

    return changed;
}



static bool mergeSimilarOccupiedNodes(Graph* graph)
{
    NodeCount numNodes = graph->numNodes;
    Node* nodes = graph->nodes;

    // reset inside on all nodes of interest, used later to mark which ones have been checked
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];

        if (node->nodeID == NULL_ID) continue;
        if (!(node->flags & FLAG_OCCUPIED)) continue;

        node->flags &= ~FLAG_INSIDE;
    }

    // loop over every node, check for nearby nodes that have the same edge list, and merge them
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* nodeA = &nodes[n];

        if (nodeA->nodeID == NULL_ID) continue;
        if (!(nodeA->flags & FLAG_OCCUPIED)) continue;

        // node is non-null and occupied

        nodeA->flags |= FLAG_INSIDE;

        // loop over all edges
        NodeList edgesA = nodeA->edges;
        for (NodeCount eA = 0; eA < edgesA.num; eA++)
        {
            bool merged = false;

            // loop over all edges of current heighbor
            NodeList* edgesT = &nodes[edgesA.list[eA]].edges; // pointer because it can change from the merging of groups
            for (NodeCount eT = 0; eT < edgesT->num; eT++)
            {
                Node* nodeB = &nodes[edgesT->list[eT]];

                if (!(nodeB->flags & FLAG_OCCUPIED)) continue;
                if (nodeB->flags & FLAG_INSIDE) continue;
                if (!nodeListsAreTheSame(&nodeA->edges, &nodeB->edges)) continue;

                // neighbor of neighbor is different from myself, has not been previously checked, and has the same edges as myself

                // merge these two nodes
                NodeID ids[2] = { nodeA->nodeID, nodeB->nodeID };
                NodeList groupList = (NodeList){ .list = ids, .num = 2 };
                mergeNodeListIntoGroup(graph, &groupList);
                eT--;

                merged = true;
                break;
            }
            
            if (merged)
            {
                changed = true;
                break;
            }
        }
    }
    
    return changed;
}



static bool mergeDeadEnds(Graph* graph, BFSContext* bfs)
{
    NodeCount numNodes = graph->numNodes;
    Node* nodes = graph->nodes;

    NodeID horse = graph->horse;
    NodeID unicorn = graph->unicorn;

    // reset inside flag on all nodes, used later to mark which nodes have been checked
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        if (node->nodeID == NULL_ID) continue;
        node->flags &= ~FLAG_INSIDE;
    }

    // mark unicorn and horse as checked
    if (horse != NULL_ID) nodes[horse].flags |= FLAG_INSIDE;
    if (unicorn != NULL_ID) nodes[unicorn].flags |= FLAG_INSIDE;

    setBFSArgs(graph, FLAG_OUTSIDE, FLAG_INSIDE, true);

    // temp buffer for storing a path
    NodeID* path = (NodeID*)calloc(numNodes, sizeof(NodeID));

    // loop over all nodes
    bool changed = false;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* node = &nodes[n];
        NodeID id = node->nodeID;

        if (id == NULL_ID) continue;
        if (node->flags & FLAG_INSIDE) continue;

        // node is non-null and has not been part of any previous search

        // get shortest path from node to horse/unicorn
        singleBFS(id);

        NodeID current, next;
        recoverReversePath(path, &current);

        // skip first if its horse/unicorn
        if (current == horse || current == unicorn)
            current = path[current];

        // for each tile along the path
        next = path[current];
        while (next != NULL_ID)
        {
            // block the current node and try to find another shortest path
            nodes[current].flags |= FLAG_OUTSIDE;
            singleBFS(id);

            // if there is another shorest path, this is not a deadend
            if (bfs->data.endReached)
            {
                nodes[current].flags ^= FLAG_OUTSIDE | FLAG_INSIDE;
                current = next;
                next = path[current];
                continue;
            }

            // check if the deadend contains a required/optional end
            setBFSArgs(graph, FLAG_OUTSIDE, FLAG_REQUIRED | FLAG_OPTIONAL, true);
            singleBFS(id);

            bool merged = false;
            if (bfs->data.endReached)
            {
                // deadend contains required/optional end

                // blocked tile can merge with adjacent occupied nodes in deadend as horse cant approach from that direction
                NodeList edges = nodes[current].edges;
                NodeList group = (NodeList)
                {
                    .num = 1,
                    .list = path,
                };

                // need visited from two BFS ago, so could be either current state or last state
                BFSState state1 = bfs->data.state;
                BFSState state2 = state1 == 1 ? MAX_BFS_STATE : state1 - 1;

                // scan all edges for occupied nodes in deadend
                for (NodeCount e = 0; e < edges.num; e++)
                {
                    NodeID edgeID = edges.list[e];
                    if (!(nodes[edgeID].flags & FLAG_OCCUPIED)) continue;
                    
                    BFSState visitedState = bfs->data.visited[edgeID];
                    if (visitedState != state1 && visitedState != state2) continue;

                    // node is occupied and in deadend

                    group.list[group.num++] = edgeID;
                }

                // node is mergable, so add current to zeroth index and merge
                if (group.num > 1)
                {
                    group.list[0] = current;
                    mergeNodeListIntoGroup(graph, &group);
                    merged = true;
                }
            }
            else
            {
                // deadend does not contain required/optional end, the entire thing can be merged together

                // get list of visited nodes and merge
                NodeList group = (NodeList)
                {
                    .num = bfs->data.head,
                    .list = bfs->data.queue,
                };
                group.list[group.num++] = group.list[0];
                group.list[0] = current;

                mergeNodeListIntoGroup(graph, &group);
                merged = true;
            }

            // reset args for finding next path
            setBFSArgs(graph, FLAG_OUTSIDE, FLAG_INSIDE, true);

            // skip the rest of this path even if there is more work that could be done
            // merging could have messed up the path, making it invalid, and anything will get caught on the next pass anyways
            if (merged)
            {
                changed = true;
                nodes[current].flags |= FLAG_INSIDE;
                break;
            }

            // remove current blocker and set to inside
            nodes[current].flags ^= FLAG_OUTSIDE | FLAG_INSIDE;
            current = next;
            next = path[current];
        }
        nodes[current].flags |= FLAG_INSIDE;
    }

    free(path);
    return changed;
}



// small helpers



static void connectNodes(Node* a, Node* b)
{
    NodeList* edgesA = &a->edges;
    NodeList* edgesB = &b->edges;
    edgesA->list[edgesA->num++] = b->nodeID;
    edgesB->list[edgesB->num++] = a->nodeID;
}



static void disconnectNodes(Node* a, Node* b)
{
    removeIDFromNodeList(&a->edges, b->nodeID);
    removeIDFromNodeList(&b->edges, a->nodeID);
}



static bool removeIDFromNodeList(NodeList* list, NodeID id)
{
    NodeList l = *list;

    for (NodeCount n = 0; n < l.num; n++)
    {
        if (l.list[n] != id) continue;

        l.list[n] = l.list[l.num - 1];
        list->num--;
        return true;
    }

    return false;
}



static bool addIDToNodeList(NodeList* list, NodeID id)
{
    NodeList l = *list;

    for (NodeCount n = 0; n < l.num; n++)
        if (l.list[n] == id)
            return false;

    l.list[list->num++] = id;
    return true;
}



static bool nodeListsAreTheSame(NodeList *listA, NodeList *listB)
{
    NodeList a = *listA;
    NodeList b = *listB;

    if (a.num != b.num)
        return false;

    for (NodeCount i = 0; i < a.num; i++)
    {
        bool found = false;

        for (NodeCount j = 0; j < b.num; j++)
        {
            if (a.list[i] == b.list[j])
            {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}



// group merging



// merges a nodelist into a single group node, first id in the list becomes the group node
static void mergeNodeListIntoGroup(Graph* graph, NodeList* group)
{
    Node* nodes = graph->nodes;
    GraphTile* tiles = graph->tiles;
    TileGroup* groups = graph->groups;

    NodeList g = *group;

    // first pass over nodes
    NodeCount maxEdges = 0;
    NodeCount maxLabelTiles = 0;
    for (NodeCount n = 0; n < g.num; n++)
    {
        Node* node = &nodes[g.list[n]];
        GraphTile* tile = &tiles[node->tileID];

        // get worst case max edges (all unique)
        maxEdges += node->edges.num;
        node->nodeID = NULL_ID;

        // get worst case max label tiles (all label tiles)
        if (tile->type == GTT_GROUP)
        {
            TileGroup* group = tile->group;
            group->groupID = NULL_ID;
            maxLabelTiles += group->labelTiles.num;
        }
        else maxLabelTiles += 1;
    }

    NodeList newEdges = (NodeList){ .num = 0, .list = (NodeID*)calloc(maxEdges, sizeof(NodeID)) };
    NodeList newLabelTiles = (NodeList){ .num = 0, .list = (NodeID*)calloc(maxLabelTiles, sizeof(NodeID)) };

    NodeID horse = graph->horse;
    NodeID unicorn = graph->unicorn;

    // loop over all nodes in group
    bool hasHorse = false;
    bool hasUnicorn = false;
    bool hasRequired = false;
    Score value = 0;
    for (NodeCount n = 0; n < g.num; n++)
    {
        NodeID id = g.list[n];
        Node* node = &nodes[id];
        NodeFlags flags = node->flags;

        // get aggregated flags and value
        hasHorse |= id == horse;
        hasUnicorn |= id == unicorn;
        hasRequired |= flags & FLAG_REQUIRED;
        value += node->value;

        // get list of unique edges to outside nodes
        NodeList edges = node->edges;
        for (NodeCount e = 0; e < edges.num; e++)
        {
            NodeID nextID = edges.list[e];
            Node* next = &nodes[nextID];

            // merging nodes have been marked null
            if (next->nodeID == NULL_ID) continue;

            addIDToNodeList(&newEdges, nextID);
            removeIDFromNodeList(&next->edges, id);
        }
        free(edges.list);

        // get label tiles (only outer tiles)
        NodeCount tileID = node->tileID;
        GraphTile* tile = &tiles[tileID];
        if (tile->type == GTT_GROUP)
        {
            NodeList labelTiles = tile->group->labelTiles;
            memcpy(newLabelTiles.list + newLabelTiles.num, labelTiles.list, sizeof(NodeID) * labelTiles.num);
            newLabelTiles.num += labelTiles.num;
            free(labelTiles.list);
        }
        else newLabelTiles.list[newLabelTiles.num++] = tileID;
    }

    NodeID newNodeID = g.list[0];
    NodeID newTileID = newLabelTiles.list[0];
    NodeID newGroupID = getNewGroupID(graph);

    NodeFlags newFlags = (nodes[newNodeID].flags & (FLAG_OCCUPIED | FLAG_SOLID)) |
                         (hasRequired ? FLAG_REQUIRED : 0) |
                         (value < 0   ? FLAG_OPTIONAL : 0);

    // make new node
    Node* newNode = &nodes[newNodeID];
    *newNode = (Node)
    {
        .nodeID = newNodeID,
        .tileID = newTileID,
        .value = value,
        .flags = newFlags,
        .edges = newEdges,
    };

    // make new group
    TileGroup* newGroup = &groups[newGroupID];
    *newGroup = (TileGroup)
    {
        .groupID = newGroupID,
        .nodeID = newNodeID,
        .labelTiles = newLabelTiles,
    };

    // add self to edge node edge lists
    for (NodeCount e = 0; e < newEdges.num; e++)
    {
        NodeList* edges = &nodes[newEdges.list[e]].edges;
        edges->list[edges->num++] = newNodeID;
    }

    // set label tiles to group label
    for (NodeCount t = 0; t < newLabelTiles.num; t++)
    {
        tiles[newLabelTiles.list[t]] = (GraphTile)
        {
            .type = GTT_GROUP,
            .nodeID = NULL_ID,
            .group = &groups[newGroupID],
        };
    }
    // remove inner group labels
    removeInnerGroupLabelTiles(graph, &newGroup->labelTiles);
    newNode->tileID = newLabelTiles.list[0];

    // update horse/unicorn
    if (hasHorse) graph->horse = newNodeID;
    if (hasUnicorn) graph->unicorn = newNodeID;
}



static void removeInnerGroupLabelTiles(Graph* graph, NodeList* labelTiles)
{
    GraphTile* tiles = graph->tiles;
    uint8_t width = graph->width;
    uint8_t height = graph->height;

    NodeList labels = *labelTiles;
    NodeID groupID = tiles[labels.list[0]].group->groupID;

    // loop over labelTiles
    NodeCount write = 0;
    for (NodeCount read = 0; read < labels.num; read++)
    {
        NodeID tileID = labels.list[read];

        bool innerNode = true;
        for (uint8_t i = 0; i < 4; i++)
        {
            // get surrounding 4 tiles over 4 iterations (misses portal connections but since there is a portal on both sides, its fine)
            NodeID nextID;
            switch (i)
            {
                case 0:
                    if (tileID / width == 0) continue;
                    nextID = tileID - width;
                    break;
                case 1:
                    if (tileID % width == 0) continue;
                    nextID = tileID - 1;
                    break;
                case 2:
                    if (tileID % width == width - 1) continue;
                    nextID = tileID + 1;
                    break;
                case 3:
                    if (tileID / width == height - 1) continue;
                    nextID = tileID + width;
                    break;
            }

            GraphTile* tile = &tiles[nextID];
            if (tile->type == GTT_NULL) continue;

            // check if neighbor tile is a node that isnt in the same group as the current one
            if (tile->type == GTT_NODE || (tile->type == GTT_GROUP && tile->group->groupID != groupID))
            {
                labels.list[write++] = tileID;
                innerNode = false;
                break;
            }
        }

        // with no important neighbors, this node is nullified
        if (innerNode)
            tiles[tileID].type = GTT_NULL;
    }

    labelTiles->num = write;
}



static NodeID getNewGroupID(Graph* graph)
{
    NodeCount numGroups = graph->numGroups;
    TileGroup* groups = graph->groups;

    for (NodeCount g = 0; g < numGroups; g++)
        if (groups[g].groupID == NULL_ID)
            return g;

    return graph->numGroups++;
}



// defrag



#define ALIGN_UP(off, align) (((off) + (align) - 1) & ~((size_t)(align) - 1))

static void defragGraph(Graph* graph)
{
    NodeCount numNodes = graph->numNodes;
    NodeCount numGroups = graph->numGroups;
    NodeCount numTiles = (NodeCount)graph->width * graph->height;

    // count remaining objects for correct sizing later

    // used to map old to new
    NodeID* nodeRemap = (NodeID*)malloc(numNodes * sizeof(NodeID));
    NodeID* groupRemap = (NodeID*)malloc(numGroups * sizeof(NodeID));

    // count nodes and edges
    NodeCount liveNodes = 0;
    NodeCount totalEdges = 0;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        if (graph->nodes[n].nodeID == NULL_ID)
        {
            nodeRemap[n] = NULL_ID;
            continue;
        }
        nodeRemap[n] = liveNodes++;
        totalEdges += graph->nodes[n].edges.num;
    }

    // count groups and labelTiles
    NodeCount liveGroups = 0;
    NodeCount totalLabelTiles = 0;
    for (NodeCount g = 0; g < numGroups; g++)
    {
        if (graph->groups[g].groupID == NULL_ID)
        {
            groupRemap[g] = NULL_ID;
            continue;
        }
        groupRemap[g] = liveGroups++;
        totalLabelTiles += graph->groups[g].labelTiles.num;
    }

    // compute total size
    size_t sizeNodes  = (size_t)liveNodes * sizeof(Node);
    size_t offEdges   = ALIGN_UP(sizeNodes, _Alignof(NodeID));
    size_t sizeEdges  = (size_t)totalEdges * sizeof(NodeID);
    size_t offGroups  = ALIGN_UP(offEdges + sizeEdges, _Alignof(TileGroup));
    size_t sizeGroups = (size_t)liveGroups * sizeof(TileGroup);
    size_t offLabels  = ALIGN_UP(offGroups + sizeGroups, _Alignof(NodeID));
    size_t sizeLabels = (size_t)totalLabelTiles * sizeof(NodeID);
    size_t offTiles   = ALIGN_UP(offLabels + sizeLabels, _Alignof(GraphTile));
    size_t sizeTiles  = (size_t)numTiles * sizeof(GraphTile);
    size_t totalSize  = offTiles + sizeTiles;

    uint8_t* buffer = (uint8_t*)malloc(totalSize);

    // new buffers
    Node*      newNodes      = (Node*)(buffer);
    NodeID*    newEdges      = (NodeID*)(buffer + offEdges);
    TileGroup* newGroups     = (TileGroup*)(buffer + offGroups);
    NodeID*    newLabelTiles = (NodeID*)(buffer + offLabels);
    GraphTile* newTiles      = (GraphTile*)(buffer + offTiles);

    // copy nodes and edges to new buffer
    NodeCount writeEdge = 0;
    for (NodeCount n = 0; n < numNodes; n++)
    {
        Node* oldNode = &graph->nodes[n];
        if (oldNode->nodeID == NULL_ID) continue;

        // get new id
        NodeID newID = nodeRemap[n];
        Node* newNode = &newNodes[newID];

        // copy over
        *newNode = *oldNode;
        newNode->nodeID = newID;

        // get pointer to edge buffer
        NodeCount edgeCount = oldNode->edges.num;
        NodeID* edgeList = &newEdges[writeEdge];

        // copy and remap edges
        for (NodeCount e = 0; e < edgeCount; e++)
            edgeList[e] = nodeRemap[oldNode->edges.list[e]];

        newNode->edges.list = edgeList;
        newNode->edges.num = edgeCount;

        free(oldNode->edges.list);
        writeEdge += edgeCount;
    }

    // copy groups and labelTiles to new buffer
    NodeCount labelCursor = 0;
    for (NodeCount g = 0; g < numGroups; g++)
    {
        TileGroup* oldGroup = &graph->groups[g];
        if (oldGroup->groupID == NULL_ID) continue;

        // get new id
        NodeID newGID = groupRemap[g];
        NodeCount labelCount = oldGroup->labelTiles.num;
        NodeID* labelList = &newLabelTiles[labelCursor];

        // copy over
        memcpy(labelList, oldGroup->labelTiles.list, labelCount * sizeof(NodeID));
        free(oldGroup->labelTiles.list);

        newGroups[newGID] = (TileGroup)
        {
            .groupID = newGID,
            .nodeID = nodeRemap[oldGroup->nodeID],
            .labelTiles = (NodeList){ .num = labelCount, .list = labelList },
        };

        labelCursor += labelCount;
    }

    // copy tiles into buffer without remapping (intended to have gaps)
    memcpy(newTiles, graph->tiles, (size_t)numTiles * sizeof(GraphTile));

    for (NodeCount t = 0; t < numTiles; t++)
    {
        GraphTile* tile = &newTiles[t];

        if (tile->type == GTT_NODE)
            tile->nodeID = nodeRemap[tile->nodeID];
        else if (tile->type == GTT_GROUP)
            tile->group = &newGroups[groupRemap[tile->group->groupID]];
    }

    // fix horse/unicorn
    if (graph->horse != NULL_ID)   graph->horse   = nodeRemap[graph->horse];
    if (graph->unicorn != NULL_ID) graph->unicorn = nodeRemap[graph->unicorn];

    // swap in new buffers
    free(graph->nodes);
    free(graph->groups);
    free(graph->tiles);
    free(nodeRemap);
    free(groupRemap);

    graph->buffer = buffer;
    graph->nodes = newNodes;
    graph->edges = newEdges;
    graph->numEdges = totalEdges;
    graph->groups = newGroups;
    graph->numGroups = liveGroups;
    graph->numNodes = liveNodes;
    graph->tiles = newTiles;
}