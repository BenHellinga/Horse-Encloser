#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "solver.h"



// typedefs



typedef uint16_t StateID;



typedef struct
{
    bool valid;
    int16_t score;
    NodeID endID;
    NodeID pathLength;
    NodeID* path;
}
ValidatorResult;



typedef struct
{
    NodeID* queue;
    StateID* visited;
    StateID state;
    NodeID* paths;
    ValidatorResult* result;
}
BFSData;



typedef void (*ValidatorFunction)(ValidatorResult* result);



typedef struct
{
    Graph* graph;
    uint8_t* endTiles;
    NodeID* currentWalls;
    uint8_t wallsUsed;
    NodeID numTiles;
    ValidatorFunction validate;
    ValidatorResult* results;
    uint8_t* checkedTiles;
    Result* best;
}
RecursionData;



// globals



static BFSData bfsData;
static RecursionData recursionData;



// functions



static void initRecursionData(Graph* graph, Result* result);
static void freeRecursionData();
static void initBFSData(Graph* graph);
static void freeBFSData();

static uint8_t* locateEndTiles(Graph* graph);
static ValidatorFunction pickValidator(GamemodeType gamemode);

static void recurse(uint8_t depth);
static void recordIfBest(int16_t score, uint8_t numWalls);

static void validateClassic(ValidatorResult* result);
static void validateCostly(ValidatorResult* result);
static void validateLovebirds(ValidatorResult* result);
static void validateQuarrel(ValidatorResult* result);

static void runBFS(NodeID startID);
static void recoverBFSPath(NodeID exitID);
static void removeCheckedTilesFromPath();



// solving



// brute forces every wall combination up to graph->numWalls, fills result with the best found
void solve(Graph* graph, Result* result)
{
    initRecursionData(graph, result);
    initBFSData(graph);

    // recursively brute force all wall combinations
    recurse(0);

    freeRecursionData();
    freeBFSData();
}



// setup



// populates recursionData for the given graph/result: wallable candidates, end flags, validator, etc.
static void initRecursionData(Graph* graph, Result* result)
{
    NodeID numTiles = (NodeID)(graph->width * graph->height);
    uint8_t numWalls = graph->numWalls;

    recursionData = (RecursionData){
        .graph = graph,
        .endTiles = locateEndTiles(graph),
        .currentWalls = (NodeID*)calloc(numWalls, sizeof(NodeID)),
        .wallsUsed = 0,
        .numTiles = numTiles,
        .validate = pickValidator(graph->gamemode),
        .results = (ValidatorResult*)calloc(numWalls + 1, sizeof(ValidatorResult)),
        .checkedTiles = (uint8_t*)calloc(numTiles, sizeof(uint8_t)),
        .best = result,
    };

    for (uint8_t w = 0; w <= numWalls; ++w)
        recursionData.results[w].path = (NodeID*)calloc(numTiles, sizeof(NodeID));

    result->solutionFound = false;
    result->score = 0;
    result->numWalls = 0;
    result->walls = malloc(sizeof(NodeID) * numWalls);
}



static void freeRecursionData()
{
    free(recursionData.endTiles);
    free(recursionData.currentWalls);
    free(recursionData.checkedTiles);

    uint8_t numWalls = recursionData.graph->numWalls;
    for (uint8_t w = 0; w <= numWalls; ++w)
        free(recursionData.results[w].path);
    free(recursionData.results);
}



// allocates the BFS queue/visited buffers, sized for numTiles
static void initBFSData(Graph* graph)
{
    NodeID numTiles = (NodeID)(graph->width * graph->height);

    bfsData = (BFSData){
        .queue = (NodeID*)calloc(numTiles, sizeof(NodeID)),
        .visited = (StateID*)calloc(numTiles, sizeof(StateID)),
        .state = 0,
        .paths = (NodeID*)calloc(numTiles, sizeof(NodeID)),
    };
}



static void freeBFSData()
{
    free(bfsData.queue);
    free(bfsData.visited);
}



// init helpers



// builds a flat width*height array flagging which tiles sit on the border
static uint8_t* locateEndTiles(Graph* graph)
{
    NodeID numTiles = (NodeID)(graph->width * graph->height);
    uint8_t* flags = malloc(sizeof(uint8_t) * numTiles);

    // end nodes are the ones on the edge of the board
    for (int y = 0; y < graph->height; ++y)
    for (int x = 0; x < graph->width; ++x)
    {
        int idx = y * graph->width + x;
        
        if (x == 0 || y == 0 || x == graph->width - 1 || y == graph->height - 1)
            flags[idx] = 1;
        else if (graph->nodes[idx].points < 0)
            flags[idx] = 2;
        else
            flags[idx] = 0;
    }

    if (graph->gamemode == GAMEMODE_QUARREL)
        flags[graph->unicorn] = 1;

    return flags;
}



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



// search



// recursively places walls to block the horse from edge tiles or negative tiles once enclosed
static void recurse(uint8_t depth)
{
    // printGraphChars(recursionData.graph);
    // getc(stdin);

    ValidatorResult* result = &recursionData.results[recursionData.wallsUsed];
    recursionData.validate(result);
    if (result->valid) recordIfBest(result->score, recursionData.wallsUsed);

    // nothing more to place
    if (recursionData.wallsUsed >= recursionData.graph->numWalls) return;

    removeCheckedTilesFromPath();

    ++recursionData.wallsUsed;

    NodeID endID = result->endID;
    uint8_t endType = recursionData.endTiles[endID];
    if (endType == 2) recursionData.endTiles[endID] = 1;

    NodeID pathLength = result->pathLength;
    NodeID* path = result->path;

    // iterate through all wallable candidates after previous recusion layer
    for (NodeID i = 0; i < pathLength; ++i)
    {
        NodeID wallID = path[i];

        // place wall
        recursionData.graph->nodes[wallID].type = TILE_WALL;
        recursionData.currentWalls[recursionData.wallsUsed - 1] = wallID;

        recurse(depth + 1);

        // remove wall
        recursionData.graph->nodes[wallID].type = TILE_EMPTY;
        recursionData.checkedTiles[wallID] = 1;
    }

    for (NodeID i = 0; i < pathLength; ++i)
        recursionData.checkedTiles[path[i]] = 0;

    --recursionData.wallsUsed;

    if (endType == 2)
    {
        recursionData.endTiles[endID] = 0;
        recurse(depth + 1);
        recursionData.endTiles[endID] = 2;
    }
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



// validator functions



// classic: bfs the horse, valid if enclosed, score is unchanged
static void validateClassic(ValidatorResult* result)
{
    bfsData.result = result;

    // check if horse is enclosed
    runBFS(recursionData.graph->horse);
}



// costly: bfs the horse, valid if enclosed, score is reduced by 6 per wall used
static void validateCostly(ValidatorResult* result)
{
    bfsData.result = result;

    // check if horse is enclosed
    runBFS(recursionData.graph->horse);

    result->score -= recursionData.wallsUsed * 6;
}



// lovebirds: bfs the horse, valid if enclosed AND horse can reach unicorn
static void validateLovebirds(ValidatorResult* result)
{
    Graph* graph = recursionData.graph;
    bfsData.result = result;
    
    // check if horse is enclosed and can reach unicorn
    runBFS(graph->horse);

    bool canReachUnicorn = bfsData.visited[graph->unicorn] == bfsData.state;
    result->valid &= canReachUnicorn;
}



// quarrel: bfs the horse, valid if both horse and unicorn are enclosed, but invalid if they can reach eachother
static void validateQuarrel(ValidatorResult* result)
{
    Graph* graph = recursionData.graph;
    bfsData.result = result;

    // check if horse in enclosed and can reach unicorn
    runBFS(graph->horse);

    if (!result->valid) return;

    bool canReachUnicorn = bfsData.visited[graph->unicorn] == bfsData.state;
    if (canReachUnicorn)
    {
        // horse is not allowed to reach unicorn in quarrel
        result->valid = false;
        return;
    }

    int16_t horseScore = result->score;

    // check if unicorn is enclosed
    recursionData.endTiles[graph->unicorn] = 0;
    runBFS(graph->unicorn);
    recursionData.endTiles[graph->unicorn] = 1;

    result->score += horseScore;
}



// bfs



// bfs from startID, stops early if it ever reaches an edge tile
static void runBFS(NodeID startID)
{
    Graph* graph = recursionData.graph;
    uint8_t* endTiles = recursionData.endTiles;

    // increment state instead of clearing BFSData
    if (bfsData.state == UINT16_MAX)
    {
        memset(bfsData.visited, 0, sizeof(StateID) * graph->numTiles);
        bfsData.state = 0;
    }
    ++bfsData.state;

    NodeID head = 0;
    NodeID tail = 0;

    bfsData.queue[++tail] = startID;
    bfsData.visited[startID] = bfsData.state;
    bfsData.paths[startID] = NULL_NODE_ID;

    int16_t score = 0;
    bool notEnclosed = false;
    NodeID exitID;

    // bfs until queue is empty, or an edge tile is reached
    while (head < tail)
    {
        NodeID current = bfsData.queue[++head];
        Node* node = &graph->nodes[current];

        score += node->points;

        if (endTiles[current] != 0)
        {
            // not enclosed
            notEnclosed = true;
            exitID = current;
            break;
        }

        for (uint8_t e = 0; e < node->numEdges; ++e)
        {
            NodeID next = node->edges[e];

            if (TILE_IS_SOLID(graph->nodes[next].type)) continue; // walled
            if (bfsData.visited[next] == bfsData.state) continue; // visited

            bfsData.visited[next] = bfsData.state;
            bfsData.queue[++tail] = next;
            bfsData.paths[next] = current;
        }
    }

    bfsData.result->valid = !notEnclosed;
    bfsData.result->score = score;
    if (notEnclosed)
    {
        bfsData.result->endID = exitID;
        recoverBFSPath(exitID);
    }
}



// recover the path, putting the last tile in the last index, so the start will be in the middle of the array
static void recoverBFSPath(NodeID exitID)
{
    NodeID current = exitID;
    NodeID numTiles = recursionData.numTiles;
    NodeID count = numTiles;
    NodeID* path = bfsData.result->path;

    while (current != NULL_NODE_ID)
    {
        path[--count] = current;
        current = bfsData.paths[current];
    }

    bfsData.result->pathLength = numTiles - count - 1;
}



// remove checked tiles from path, and also move path to front of array
static void removeCheckedTilesFromPath()
{
    NodeID pathLength = bfsData.result->pathLength;
    NodeID* path = bfsData.result->path;

    NodeID end = recursionData.numTiles;
    NodeID start = end - pathLength;

    NodeID count = -1;
    for (NodeID i = start; i < end; ++i)
    {
        NodeID current = path[i];

        if (recursionData.checkedTiles[current] != 0) continue;
        if (recursionData.graph->nodes[current].type != TILE_EMPTY) continue;

        path[++count] = current;
    }
    bfsData.result->pathLength = ++count;
}