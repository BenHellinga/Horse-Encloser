#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "graph.h"
#include "bfs.h"
#include "printing.h"

#include "solver.h"



// typedefs



typedef void (*ValidatorFunction)(BFSResult* result);



typedef struct
{
    Graph* graph;
    Board* board;
    GraphPrintingInfo* info;

    EndType* endMask;      // mutable: optional ends escalate to required once reached at a shallower depth
    NodeID* currentWalls;  // walls placed along the current recursion branch, indexed by depth
    uint8_t wallsUsed;
    uint8_t wallBudget;     // how many walls the current outer (iterative deepening) pass is allowed to use
    NodeCount numNodes;

    ValidatorFunction validate;

    BFSResult** depthResults; // one persistent BFSResult per recursion depth, so a deeper call can't clobber a shallower one's path
    uint8_t* checkedTiles;

    Result* best; // accumulates across every outer iterative deepening pass
}
SolverData;



// globals



static SolverData solverData;
static BFSData* bfsData;
static BFSArgs* bfsArgs;



// declarations



static void initSolverData(Graph* graph, Board* board, GraphPrintingInfo* info);
static void freeSolverData(void);

static ValidatorFunction pickValidator(Gamemode gamemode);
static void buildEndMask(void);

static void recurse(void);
static void recurse_r(uint8_t depth);
static void recordIfBest(ScoreValue score, uint8_t numWalls);
static void removeCheckedTilesFromPath(BFSResult* result);

static void validateClassic(BFSResult* result);
static void validateCostly(BFSResult* result);
static void validateLovebirds(BFSResult* result);
static void validateQuarrel(BFSResult* result);



// header functions



// solves the board, puts the best solution in result, otherwise returns -1 if no solution found
Result* solve(Board* board)
{
    Graph* graph = graphFromBoard(board);
    GraphPrintingInfo* info = getGraphPrintingInfo(graph, board);

    initSolverData(graph, board, info);

    uint8_t maxWalls = board->numWalls;

    // iterative deepening on num walls, mirroring v2: each pass is a full, independent solve
    // capped at that many walls. this duplicates work, but gives a sense of progress on slow puzzles.
    for (uint8_t numWalls = 0; numWalls <= maxWalls; ++numWalls)
    {
        solverData.wallBudget = numWalls;

        clock_t start = clock();
        recurse();
        double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;

        if (solverData.best->solutionFound)
            printf("walls: %u -> score: %d (%.3fs)\n", numWalls, solverData.best->score, elapsed);
        else
            printf("walls: %u -> no solution (%.3fs)\n", numWalls, elapsed);
    }

    Result* result = solverData.best;

    freeSolverData();
    freeGraphPrintingInfo(info);
    freeGraph(graph);

    return result;
}



// frees a result
void freeResult(Result* result)
{
    free(result->walls);
    free(result);
}



// setup



static void initSolverData(Graph* graph, Board* board, GraphPrintingInfo* info)
{
    NodeCount numNodes = graph->numNodes;
    uint8_t maxWalls = board->numWalls;
    uint8_t maxWallsAlloc = maxWalls > 0 ? maxWalls : 1;

    solverData = (SolverData)
    {
        .graph = graph,
        .board = board,
        .info = info,
        .endMask = (EndType*)calloc(numNodes, sizeof(EndType)),
        .currentWalls = (NodeID*)calloc(maxWallsAlloc, sizeof(NodeID)),
        .wallsUsed = 0,
        .wallBudget = 0,
        .numNodes = numNodes,
        .validate = pickValidator(board->gamemode),
        .checkedTiles = (uint8_t*)calloc(numNodes, sizeof(uint8_t)),
        .depthResults = (BFSResult**)calloc(maxWalls + 1, sizeof(BFSResult*)),
    };

    for (uint8_t d = 0; d <= maxWalls; ++d)
        solverData.depthResults[d] = initBFSResult(numNodes);

    solverData.best = (Result*)calloc(1, sizeof(Result));
    solverData.best->solutionFound = false;
    solverData.best->walls = (TileIndex*)calloc(maxWallsAlloc, sizeof(TileIndex));

    bfsData = initBFSData(numNodes);
    bfsArgs = initBFSArgs(graph);
    bfsArgs->startList = (NodeID*)calloc(1, sizeof(NodeID)); // solver never bfs's from more than one start at a time
    bfsArgs->endMask = solverData.endMask;
}



static void freeSolverData(void)
{
    free(solverData.endMask);
    free(solverData.currentWalls);
    free(solverData.checkedTiles);

    uint8_t maxWalls = solverData.board->numWalls;
    for (uint8_t d = 0; d <= maxWalls; ++d)
        freeBFSResult(solverData.depthResults[d]);
    free(solverData.depthResults);

    free(bfsArgs->startList);
    freeBFSData(bfsData);
    freeBFSArgs(bfsArgs);

    // solverData.best is returned to the caller, not freed here
}



static ValidatorFunction pickValidator(Gamemode gamemode)
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



static void buildEndMask(void)
{
    Graph* graph = solverData.graph;
    NodeCount numNodes = solverData.numNodes;
    EndType* endMask = solverData.endMask;

    memset(endMask, END_NONE, numNodes * sizeof(EndType));

    for (NodeCount r = 0; r < graph->numRequired; ++r)
        endMask[graph->requiredEnds[r]] = END_REQUIRED;

    for (NodeCount o = 0; o < graph->numOptional; ++o)
        endMask[graph->optionalEnds[o]] = END_OPTIONAL;

    // quarrel: horse must never reach the unicorn's tile, so treat it like a border
    if (solverData.board->gamemode == GAMEMODE_QUARREL)
        endMask[graph->unicorn] = END_REQUIRED;
}



// search



static void recurse(void)
{
    solverData.wallsUsed = 0;
    memset(solverData.checkedTiles, 0, sizeof(uint8_t) * solverData.numNodes);
    buildEndMask();

    recurse_r(0);
}



// recursively places walls to block the horse from required/optional ends once enclosed
static void recurse_r(uint8_t depth)
{
    BFSResult* result = solverData.depthResults[solverData.wallsUsed];
    solverData.validate(result);

    if (!result->endReached) recordIfBest(result->score, solverData.wallsUsed);

    // nothing more to place
    if (solverData.wallsUsed >= solverData.wallBudget) return;

    removeCheckedTilesFromPath(result);

    ++solverData.wallsUsed;

    NodeID endID = result->endID;
    EndType endType = solverData.endMask[endID];
    if (endType == END_OPTIONAL) solverData.endMask[endID] = END_REQUIRED;

    NodeCount pathLength = result->pathLength;
    NodeID* path = result->path;

    for (NodeCount i = 0; i < pathLength; ++i)
    {
        NodeID wallID = path[i];

        // place wall
        solverData.graph->nodes[wallID].type = NODE_UNWALKABLE;
        solverData.currentWalls[solverData.wallsUsed - 1] = wallID;

        recurse_r(depth + 1);

        // remove wall
        solverData.graph->nodes[wallID].type = NODE_NONE;
        solverData.checkedTiles[wallID] = 1;
    }

    for (NodeCount i = 0; i < pathLength; ++i)
        solverData.checkedTiles[path[i]] = 0;

    --solverData.wallsUsed;

    if (endType == END_OPTIONAL)
    {
        solverData.endMask[endID] = END_NONE;
        recurse_r(depth + 1);
        solverData.endMask[endID] = END_OPTIONAL;
    }
}



static void recordIfBest(ScoreValue score, uint8_t numWalls)
{
    Result* best = solverData.best;

    bool isBetter = !best->solutionFound ||
                     score > best->score ||
                    (score == best->score && numWalls < best->numWalls);
    if (!isBetter) return;

    best->solutionFound = true;
    best->score = score;
    best->numWalls = numWalls;

    // walls are only ever placed on NODE_NONE nodes, which are never part of a merged group,
    // so nodeID doubles as the tile index directly
    for (uint8_t i = 0; i < numWalls; ++i)
        best->walls[i] = (TileIndex)solverData.currentWalls[i];
}



// removes already-checked or unplaceable tiles from a bfs path, and compacts what remains to the front
static void removeCheckedTilesFromPath(BFSResult* result)
{
    NodeCount pathLength = result->pathLength;
    NodeID* path = result->path;

    NodeCount end = solverData.numNodes;
    NodeCount start = end - pathLength;

    uint8_t* checkedTiles = solverData.checkedTiles;
    Node* nodes = solverData.graph->nodes;

    NodeCount count = 0;
    for (NodeCount i = start; i < end; i++)
    {
        NodeID current = path[i];

        if (checkedTiles[current] != 0) continue;
        if (nodes[current].type != NODE_NONE) continue;

        path[count++] = current;
    }

    result->pathLength = count;
}



// validator functions



// classic: bfs the horse, valid if enclosed, score is unchanged
static void validateClassic(BFSResult* result)
{
    bfsArgs->numStarts = 1;
    bfsArgs->startList[0] = solverData.graph->horse;
    bfsArgs->stopEarly = true;
    bfsArgs->includeEnds = true;

    BFS(bfsData, bfsArgs, result);
}



// costly: bfs the horse, valid if enclosed, score is reduced by 6 per wall used
static void validateCostly(BFSResult* result)
{
    validateClassic(result);

    result->score -= solverData.wallsUsed * 6;
}



// bfs the horse, valid if enclosed AND horse can reach unicorn
static void validateLovebirds(BFSResult* result)
{
    validateClassic(result);

    if (result->endReached) return;

    bool canReachUnicorn = bfsData->visited[solverData.graph->unicorn] == bfsData->state;
    if (!canReachUnicorn)
        result->endReached = true; // invalid; endID/path are left stale from this depth's last real bfs
}



// bfs the horse, valid if both horse and unicorn are enclosed, but invalid if they can reach each other
static void validateQuarrel(BFSResult* result)
{
    Graph* graph = solverData.graph;

    // check if horse is enclosed and can reach unicorn
    validateClassic(result);

    if (result->endReached) return;

    bool canReachUnicorn = bfsData->visited[graph->unicorn] == bfsData->state;
    if (canReachUnicorn)
    {
        // horse is not allowed to reach unicorn in quarrel
        result->endReached = true;
        return;
    }

    ScoreValue horseScore = result->score;

    // check if unicorn is enclosed
    solverData.endMask[graph->unicorn] = END_NONE;

    bfsArgs->numStarts = 1;
    bfsArgs->startList[0] = graph->unicorn;
    bfsArgs->stopEarly = true;
    bfsArgs->includeEnds = true;

    BFS(bfsData, bfsArgs, result);

    solverData.endMask[graph->unicorn] = END_REQUIRED;

    if (!result->endReached)
        result->score += horseScore;
}