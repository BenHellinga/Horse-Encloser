#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "parsing.h"



// global



const char TILE_CHARS[NUM_TILE_TYPES] = TILE_TYPES_TO_CHAR;
const int8_t TILE_POINTS[NUM_TILE_TYPES] = TILE_TYPES_TO_POINTS;
const char* GAMEMODE_STRINGS[NUM_GAMEMODES] = GAMEMODE_TYPES_TO_STRING;



// typedefs



typedef struct
{
    char* board;
    int* portalPartner;
    GamemodeType gamemode;
    int numWalls;
    int width;
    int height;
    int horseIdx;
    int unicornIdx;
}
ParsingInfo;



#define NEW_PARSING_INFO { 0 }
#define FREE_PARSING_INFO(info) free((info)->board); free((info)->portalPartner);



// functions



static ReturnCode readBoard(const char* filepath, ParsingInfo* info);
static ReturnCode gamemodeFromString(const char* str, GamemodeType* gamemode);
static ReturnCode locateSpecialTiles(ParsingInfo* info);
static void buildGraph(const ParsingInfo* info, Graph* graph);

static void bfsMark(Graph* graph, const int* portalPartner, int* queue, int width, int height, int startIdx, NodeID* count);
static void visitTile(Graph* graph, int* queue, int* tail, NodeID* count, int idx);
static void buildEdges(Graph* graph, const int* portalPartner, int width, int height);
static void addEdge(Graph* graph, NodeID nodeA, NodeID nodeB);

static TileType tileTypeFromChar(char c);
static bool isEdgeTile(int x, int y, int width, int height);
static bool isOffBoard(int x, int y, int width, int height);



// graph parsing



// reads filepath and fills graph, returns error if the file is invalid
ReturnCode parseGraph(const char* filepath, Graph* graph)
{
    ParsingInfo info = NEW_PARSING_INFO;

    // read file into info.board, along with the gamemode and header fields
    if (readBoard(filepath, &info) == ERROR)
    {
        FREE_PARSING_INFO(&info);
        return ERROR;
    }

    // locate horse, unicorn, and portal destinations, and validate them against the gamemode
    if (locateSpecialTiles(&info) == ERROR)
    {
        FREE_PARSING_INFO(&info);
        return ERROR;
    }

    // convert board and special tile locations to graph
    buildGraph(&info, graph);
    graph->gamemode = info.gamemode;
    graph->numWalls = info.numWalls;

    FREE_PARSING_INFO(&info);

    return SUCCESS;
}



// reads the file version, gamemode, header, and tile grid from filepath into a freshly allocated board buffer
static ReturnCode readBoard(const char* filepath, ParsingInfo* info)
{
    FILE* file = fopen(filepath, "r");

    if (file == NULL)
    {
        fprintf(stderr, "could not open '%s'\n", filepath);
        return ERROR;
    }

    int version;

    if (fscanf(file, "%d", &version) != 1)
    {
        fprintf(stderr, "could not read version from '%s'\n", filepath);
        fclose(file);
        return ERROR;
    }

    if (version != 1)
    {
        fprintf(stderr, "unsupported version '%d' in '%s'\n", version, filepath);
        fclose(file);
        return ERROR;
    }

    // skip to the next line before reading the gamemode
    char c;
    while ((c = fgetc(file)) != '\n' && c != EOF);

    char gamemodeStr[GAMEMODE_STRING_BUFFER_SIZE];

    if (fscanf(file, "%31s", gamemodeStr) != 1)
    {
        fprintf(stderr, "could not read gamemode from '%s'\n", filepath);
        fclose(file);
        return ERROR;
    }

    if (gamemodeFromString(gamemodeStr, &info->gamemode) == ERROR)
    {
        fprintf(stderr, "unknown gamemode '%s' in '%s'\n", gamemodeStr, filepath);
        fclose(file);
        return ERROR;
    }

    // skip to the next line before reading numWalls
    while ((c = fgetc(file)) != '\n' && c != EOF);

    if (fscanf(file, "%d", &info->numWalls) != 1)
    {
        fprintf(stderr, "could not read numWalls from '%s'\n", filepath);
        fclose(file);
        return ERROR;
    }

    // skip to the next line before reading the width and height
    while ((c = fgetc(file)) != '\n' && c != EOF);

    if (fscanf(file, "%d,%d", &info->width, &info->height) != 2)
    {
        fprintf(stderr, "could not read board dimensions from '%s'\n", filepath);
        fclose(file);
        return ERROR;
    }

    // skip to the next line before reading tile rows
    while ((c = fgetc(file)) != '\n' && c != EOF);

    // read board into info->board
    info->board = malloc(sizeof(char) * info->width * info->height);
    char linebuf[PARSING_LINE_BUFFER_SIZE];

    for (int y = 0; y < info->height; ++y)
    {
        memset(linebuf, ' ', sizeof(linebuf));

        if (fgets(linebuf, sizeof(linebuf), file) == NULL)
            break;

        for (int x = 0; x < info->width; ++x)
            info->board[y * info->width + x] = linebuf[x];
    }

    fclose(file);
    return SUCCESS;
}



// maps a gamemode string to its type, error if unrecognized
static ReturnCode gamemodeFromString(const char* str, GamemodeType* gamemode)
{
    for (int i = 0; i < NUM_GAMEMODES; ++i)
    {
        if (strcmp(str, GAMEMODE_STRINGS[i]) == 0)
        {
            *gamemode = (GamemodeType)i;
            return SUCCESS;
        }
    }

    return ERROR;
}



// finds the horse, unicorn, and pairs up matching portal tiles, validates them against the gamemode
static ReturnCode locateSpecialTiles(ParsingInfo* info)
{
    int firstSeen[NUM_PORTAL_PAIRS];
    int portalCount[NUM_PORTAL_PAIRS];

    for (int i = 0; i < NUM_PORTAL_PAIRS; ++i)
    {
        firstSeen[i] = -1;
        portalCount[i] = 0;
    }

    info->horseIdx = -1;
    info->unicornIdx = -1;

    info->portalPartner = malloc(sizeof(int) * info->width * info->height);

    // loop over all tiles to find horse, unicorn, and portal pairs
    for (int i = 0; i < info->width * info->height; ++i)
    {
        info->portalPartner[i] = -1;

        char tile = info->board[i];

        if (tile == TILE_CHAR_HORSE)
            info->horseIdx = i;
        else if (tile == TILE_CHAR_UNICORN)
            info->unicornIdx = i;
        else if (tile == TILE_CHAR_BLUE_PORTAL || tile == TILE_CHAR_PINK_PORTAL || tile == TILE_CHAR_PURPLE_PORTAL)
        {
            int color = tile - TILE_CHAR_BLUE_PORTAL;
            ++portalCount[color];

            if (firstSeen[color] == -1)
                firstSeen[color] = i;
            else
            {
                info->portalPartner[i] = firstSeen[color];
                info->portalPartner[firstSeen[color]] = i;
            }
        }
    }

    // the horse must always exist
    if (info->horseIdx == -1)
    {
        fprintf(stderr, "board has no horse tile\n");
        return ERROR;
    }

    // the unicorn must exist for lovers/quarrel, and must not exist otherwise
    bool needsUnicorn = GAMEMODE_REQUIRES_UNICORN(info->gamemode);
    bool hasUnicorn = info->unicornIdx != -1;

    if (needsUnicorn && !hasUnicorn)
    {
        fprintf(stderr, "gamemode '%s' requires a unicorn tile, but the board has none\n", GAMEMODE_STRINGS[info->gamemode]);
        return ERROR;
    }

    if (!needsUnicorn && hasUnicorn)
    {
        fprintf(stderr, "gamemode '%s' does not use a unicorn tile, but the board has one\n", GAMEMODE_STRINGS[info->gamemode]);
        return ERROR;
    }

    // each portal color must appear exactly twice, or not at all
    for (int color = 0; color < NUM_PORTAL_PAIRS; ++color)
    {
        if (portalCount[color] == 1)
        {
            fprintf(stderr, "board has only one portal of color %d, expected a matching pair\n", color);
            return ERROR;
        }

        if (portalCount[color] >= 3)
        {
            fprintf(stderr, "board has %d portals of color %d, expected at most a matching pair\n", portalCount[color], color);
            return ERROR;
        }
    }

    return SUCCESS;
}



// allocates and fills every tile, then marks and connects the ones actually reachable
static void buildGraph(const ParsingInfo* info, Graph* graph)
{
    NodeID numTiles = (NodeID)(info->width * info->height);

    graph->nodes = malloc(sizeof(Node) * numTiles);

    // every tile gets its real type/points up front, whether or not it ends up reachable
    // id stays NULL_NODE_ID until bfsMark proves it's actually part of the walkable graph
    for (NodeID i = 0; i < numTiles; ++i)
    {
        TileType type = tileTypeFromChar(info->board[i]);

        graph->nodes[i].id = NULL_NODE_ID;
        graph->nodes[i].type = type;
        graph->nodes[i].points = TILE_POINTS[type];
        graph->nodes[i].numEdges = 0;
    }

    int* queue = malloc(sizeof(int) * numTiles);

    NodeID numNodes = 0;

    // perform bfs from horse to find reachable nodes
    bfsMark(graph, info->portalPartner, queue, info->width, info->height, info->horseIdx, &numNodes);

    // if not reachable from horse, perform bfs from unicorn to find reachable nodes
    if (info->unicornIdx != -1 && IS_NODE_NULL(graph->nodes[info->unicornIdx]))
        bfsMark(graph, info->portalPartner, queue, info->width, info->height, info->unicornIdx, &numNodes);

    // build edges between reachable nodes
    buildEdges(graph, info->portalPartner, info->width, info->height);

    graph->width = info->width;
    graph->height = info->height;
    graph->numNodes = numNodes;

    // horse and unicorn tile ids are their own board index, since a node's
    // id is set to its own index once bfsMark visits it, and the start
    // tile of a bfs is always visited first
    graph->horse = (NodeID)info->horseIdx;
    graph->unicorn = info->unicornIdx != -1 ? (NodeID)info->unicornIdx : NULL_NODE_ID;

    free(queue);
}



// edge building



// bfs from a start tile, marking every reachable non-solid tile as a graph member
static void bfsMark(Graph* graph, const int* portalPartner, int* queue, int width, int height, int startIdx, NodeID* count)
{
    if (!IS_NODE_NULL(graph->nodes[startIdx]) || TILE_IS_SOLID(graph->nodes[startIdx].type)) return;

    // queue head/tail
    int head = 0;
    int tail = 0;

    // init start
    visitTile(graph, queue, &tail, count, startIdx);

    // bfs until queue is empty
    while (head < tail)
    {
        int curIdx = queue[++head];
        int x = curIdx % width;
        int y = curIdx / width;

        bool curIsEdge = isEdgeTile(x, y, width, height);

        int dx[4] = { 0, 0, -1, 1 };
        int dy[4] = { -1, 1, 0, 0 };

        for (int d = 0; d < 4; ++d)
        {
            int nx = x + dx[d];
            int ny = y + dy[d];

            if (isOffBoard(nx, ny, width, height)) continue;

            // an edge tile only ever connects perpendicular to the border, never side to side along it
            if (curIsEdge && isEdgeTile(nx, ny, width, height)) continue;

            int neighborIdx = ny * width + nx;
            if (!TILE_IS_SOLID(graph->nodes[neighborIdx].type) && IS_NODE_NULL(graph->nodes[neighborIdx]))
                visitTile(graph, queue, &tail, count, neighborIdx);
        }

        // if the tile is a portal
        int partnerIdx = portalPartner[curIdx];
        if (partnerIdx != -1 && IS_NODE_NULL(graph->nodes[partnerIdx]))
            visitTile(graph, queue, &tail, count, partnerIdx);
    }
}



// marks a tile as a graph member and enqueues it for further traversal
static void visitTile(Graph* graph, int* queue, int* tail, NodeID* count, int idx)
{
    graph->nodes[idx].id = (NodeID)idx;
    ++(*count);

    queue[++(*tail)] = idx;
}



// connects every graph member to its adjacent members and portal partner
static void buildEdges(Graph* graph, const int* portalPartner, int width, int height)
{
    // loop over all nodes, connecting the ones which are reachable
    for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
        int idx = y * width + x;

        // not reachable
        if (IS_NODE_NULL(graph->nodes[idx])) continue;

        bool curIsEdge = isEdgeTile(x, y, width, height);

        // connect to the node to the right
        if (x + 1 < width && !IS_NODE_NULL(graph->nodes[idx + 1]) && !(curIsEdge && isEdgeTile(x + 1, y, width, height)))
            addEdge(graph, (NodeID)idx, (NodeID)(idx + 1));

        // connect to the node above
        if (y + 1 < height && !IS_NODE_NULL(graph->nodes[idx + width]) && !(curIsEdge && isEdgeTile(x, y + 1, width, height)))
            addEdge(graph, (NodeID)idx, (NodeID)(idx + width));

        // connect to the node through the portal
        int partnerIdx = portalPartner[idx];
        if (partnerIdx != -1 && partnerIdx > idx && !IS_NODE_NULL(graph->nodes[partnerIdx]))
            addEdge(graph, (NodeID)idx, (NodeID)partnerIdx);
    }
}



// adds a single edge in both directions
static void addEdge(Graph* graph, NodeID nodeA, NodeID nodeB)
{
    graph->nodes[nodeA].edges[graph->nodes[nodeA].numEdges++] = nodeB;
    graph->nodes[nodeB].edges[graph->nodes[nodeB].numEdges++] = nodeA;
}



// maps a board character to its tile type
static TileType tileTypeFromChar(char c)
{
    switch (c)
    {
        case TILE_CHAR_EMPTY:         return TILE_EMPTY;
        case TILE_CHAR_HORSE:         return TILE_HORSE;
        case TILE_CHAR_UNICORN:       return TILE_UNICORN;
        case TILE_CHAR_WALL:          return TILE_WALL;
        case TILE_CHAR_WATER:         return TILE_WATER;
        case TILE_CHAR_BEE:           return TILE_BEE;
        case TILE_CHAR_CHERRY:        return TILE_CHERRY;
        case TILE_CHAR_APPLE:         return TILE_APPLE;
        case TILE_CHAR_BLUE_PORTAL:   return TILE_BLUE_PORTAL;
        case TILE_CHAR_PINK_PORTAL:   return TILE_PINK_PORTAL;
        case TILE_CHAR_PURPLE_PORTAL: return TILE_PURPLE_PORTAL;
        default:                      return TILE_EMPTY;
    }
}



// true if the tile sits on the outer border of the board
static bool isEdgeTile(int x, int y, int width, int height)
{
    return x == 0 || y == 0 || x == width - 1 || y == height - 1;
}



// true if the tile coordinates fall outside the board
static bool isOffBoard(int x, int y, int width, int height)
{
    return x < 0 || x >= width || y < 0 || y >= height;
}