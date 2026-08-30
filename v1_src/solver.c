#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "solver.h"



// typedefs



typedef struct
{
    NodeID* queue;
    uint32_t* visited;
    uint32_t state;
}
BFSData;



typedef struct
{
    bool enclosed;
    int16_t score;
}
BFSResult;



typedef struct
{
    bool valid;
    int16_t score;
}
ValidatorResult;



typedef ValidatorResult (*ValidatorFunction)(void);



typedef struct
{
    Graph* graph;
    uint8_t* ends;
    NodeID* wallable;
    uint16_t numWallable;
    NodeID* currentWalls;
    uint8_t wallsUsed;
    ValidatorFunction validate;
    Result* best;
}
RecursionData;



// globals



static BFSData bfsData;
static RecursionData recursionData;



// functions



static void initRecursionData(Graph* graph, Result* result);
static void freeRecursionData(void);
static void initBFSData(Graph* graph);
static void freeBFSData(void);

static uint8_t* locateEnds(const Graph* graph);
static BFSResult runBFS(NodeID startId);
static void recurse(uint16_t start, uint8_t depth);
static void recordIfBest(int16_t score, uint8_t numWalls);

static ValidatorFunction pickValidator(GamemodeType gamemode);
static ValidatorResult validateClassic();
static ValidatorResult validateCostly();
static ValidatorResult validateLovebirds();
static ValidatorResult validateQuarrel();



// solving



// brute forces every wall combination up to graph->numWalls, fills result with the best found
void solve(Graph* graph, Result* result)
{
    initRecursionData(graph, result);
    initBFSData(graph);

    // recursively brute force all wall combinations
    recurse(0, 0);

    freeRecursionData();
    freeBFSData();

    return;
}



// setup



// populates recursionData for the given graph/result: wallable candidates, end flags, validator, etc.
static void initRecursionData(Graph* graph, Result* result)
{
    NodeID numTiles = (NodeID)(graph->width * graph->height);
    uint8_t numWalls = graph->numWalls;

    recursionData = (RecursionData){ 0 };
    recursionData.graph = graph;
    recursionData.wallable = (NodeID*)malloc(sizeof(NodeID) * numTiles);
    recursionData.currentWalls = (NodeID*)malloc(sizeof(NodeID) * numWalls);
    recursionData.validate = pickValidator(graph->gamemode);

    // wallable candidates, only empty, non-null tiles
    recursionData.numWallable = -1;
    for (NodeID i = 0; i < numTiles; ++i)
        if (!IS_NODE_NULL(graph->nodes[i]) && TILE_IS_EMPTY(graph->nodes[i].type))
            recursionData.wallable[++recursionData.numWallable] = i;

    // locate all end nodes
    recursionData.ends = locateEnds(graph);
    recursionData.best = result;

    result->solutionFound = false;
    result->score = 0;
    result->numWalls = 0;
    result->walls = malloc(sizeof(NodeID) * numWalls);
}



static void freeRecursionData(void)
{
    free(recursionData.ends);
    free(recursionData.wallable);
    free(recursionData.currentWalls);
}



// allocates the BFS queue/visited buffers, sized for numTiles
static void initBFSData(Graph* graph)
{
    NodeID numTiles = (NodeID)(graph->width * graph->height);

    bfsData = (BFSData){ NULL, NULL, 0 };
    bfsData.queue = (NodeID*)malloc(sizeof(NodeID) * numTiles);
    bfsData.visited = (uint32_t*)malloc(sizeof(uint32_t) * numTiles);

    for (uint16_t i = 0; i < numTiles; ++i)
        bfsData.visited[i] = 0;
}



static void freeBFSData(void)
{
    free(bfsData.queue);
    free(bfsData.visited);
}



// search



// checks whether the current wall arrangement is valid for the active gamemode, then
// tries every remaining wallable tile at this depth, recursing deeper
static void recurse(uint16_t start, uint8_t depth)
{
    ValidatorResult result = recursionData.validate();
    if (result.valid) recordIfBest(result.score, depth);

    // nothing more to place
    if (depth >= recursionData.graph->numWalls) return;

    ++recursionData.wallsUsed;

    // iterate through all wallable candidates after previous recusion layer
    for (uint16_t i = start; i < recursionData.numWallable; ++i)
    {
        NodeID wallId = recursionData.wallable[i];

        // place wall
        recursionData.graph->nodes[wallId].type = TILE_WALL;
        recursionData.currentWalls[depth] = wallId;

        recurse(i + 1, depth + 1);

        // remove wall
        recursionData.graph->nodes[wallId].type = TILE_EMPTY;
    }

    --recursionData.wallsUsed;
}



// stores the current wall arrangement as the new best if it beats the old best
static void recordIfBest(int16_t score, uint8_t numWalls)
{
    Result* best = recursionData.best;

    bool isBetter = !best->solutionFound ||
                     score > best->score ||
                    (score == best->score && numWalls < best->numWalls);
    if (!isBetter) return;

    best->solutionFound = true;
    best->score = score;
    best->numWalls = numWalls;

    memcpy(best->walls, recursionData.currentWalls, sizeof(NodeID) * numWalls);
}



// validators



// selects the validator function for the given gamemode
static ValidatorFunction pickValidator(GamemodeType gamemode)
{
    switch (gamemode)
    {
        case GAMEMODE_CLASSIC:   return validateClassic;
        case GAMEMODE_COSTLY:    return validateCostly;
        case GAMEMODE_LOVEBIRDS: return validateLovebirds;
        case GAMEMODE_QUARREL:   return validateQuarrel;
        default:                 return validateClassic;
    }
}



// classic: bfs the horse, valid if enclosed, score is unchanged
static ValidatorResult validateClassic()
{
    BFSResult horse = runBFS(recursionData.graph->horse);

    ValidatorResult result;
    result.valid = horse.enclosed;
    result.score = horse.score;

    return result;
}



// costly: bfs the horse, valid if enclosed, score is reduced by 6 per wall used
static ValidatorResult validateCostly()
{
    // check if horse is enclosed
    BFSResult horse = runBFS(recursionData.graph->horse);

    ValidatorResult result;
    result.valid = horse.enclosed;
    result.score = horse.score - recursionData.wallsUsed * 6;

    return result;
}



// lovebirds: bfs the horse, valid if enclosed AND horse can reach unicorn
static ValidatorResult validateLovebirds()
{
    const Graph* graph = recursionData.graph;
    
    // check if horse is enclosed and can reach unicorn
    BFSResult horse = runBFS(graph->horse);
    bool canReachUnicorn = bfsData.visited[graph->unicorn] == bfsData.state;

    ValidatorResult result;
    result.valid = horse.enclosed && canReachUnicorn;
    result.score = horse.score;

    return result;
}



// quarrel: bfs the horse, valid if both horse and unicorn are enclosed, but invalid if they can reach eachother
static ValidatorResult validateQuarrel(void)
{
    const Graph* graph = recursionData.graph;

    // check if horse in enclosed and can reach unicorn
    BFSResult horse = runBFS(graph->horse);
    bool canReachUnicorn = bfsData.visited[graph->unicorn] == bfsData.state;

    ValidatorResult result;
    result.score = horse.score;

    if (!horse.enclosed || canReachUnicorn)
    {
        // horse isn't enclosed, or it can reach the unicorn
        result.valid = false;
        return result;
    }

    // check if unicorn is enclosed
    BFSResult unicorn = runBFS(graph->unicorn);

    result.valid = unicorn.enclosed;
    result.score = horse.score + unicorn.score;

    return result;
}



// bfs



// bfs from startId, stops early if it ever reaches an edge tile
static BFSResult runBFS(NodeID startId)
{
    const Graph* graph = recursionData.graph;
    const uint8_t* ends = recursionData.ends;

    // increment state instead of clearing BFSData
    if (bfsData.state == UINT16_MAX)
    {
        memset(bfsData.visited, 0, sizeof(uint16_t) * graph->numNodes);
        bfsData.state = 0;
    }
    ++bfsData.state;

    uint16_t head = 0;
    uint16_t tail = 0;

    bfsData.queue[++tail] = startId;
    bfsData.visited[startId] = bfsData.state;

    int16_t score = 0;
    bool notEnclosed = false;

    // bfs until queue is empty, or an edge tile is reached
    while (head < tail)
    {
        NodeID current = bfsData.queue[++head];
        const Node* node = &graph->nodes[current];

        score += node->points;

        if (ends[current])
        {
            // not enclosed
            notEnclosed = true;
            break;
        }

        for (uint8_t e = 0; e < node->numEdges; ++e)
        {
            NodeID next = node->edges[e];

            if (TILE_IS_SOLID(graph->nodes[next].type)) continue; // walled
            if (bfsData.visited[next] == bfsData.state) continue; // visited

            bfsData.visited[next] = bfsData.state;
            bfsData.queue[++tail] = next;
        }
    }

    BFSResult result;
    result.enclosed = !notEnclosed;
    result.score = score;

    return result;
}



// helpers



// builds a flat width*height array flagging which tiles sit on the border
static uint8_t* locateEnds(const Graph* graph)
{
    NodeID numTiles = (NodeID)(graph->width * graph->height);
    uint8_t* flags = malloc(sizeof(uint8_t) * numTiles);

    // end nodes are the ones on the edge of the board
    for (int y = 0; y < graph->height; ++y)
    for (int x = 0; x < graph->width; ++x)
    {
        int idx = y * graph->width + x;
        flags[idx] = (x == 0 || y == 0 || x == graph->width - 1 || y == graph->height - 1) ? 1 : 0;
    }

    return flags;
}