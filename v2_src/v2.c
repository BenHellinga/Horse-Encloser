#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "v2.h"



/*
    v2 strategy revolves around finding the shortest path of escape at each level of recursion, and iterating the wall along that
    path instead of iterating every possible tile. Where v1 is (all wallable tiles)^(num walls), v2 reduces the base to (average shortest path length)^(num walls)

    Note that special care has to be taken when it comes to negative tiles, as they are not required to be blocked, but may want to be blocked. Therefore the negative tiles
    are considered end tiles, and the shortest path to them is blocked, until the path has been fully enumerated, where then the algorithm also considers if it was not blocked.

    Enumerating this way no longer finds all possible enclosements, but narrows the search by cutting some obviously unoptimal ones.

    The code for v2 got a little messy as I was trying to finish it quickly so I could progress to v3, as I knew v2 would not be fast enough for all puzzles.
*/



// main



int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filepath>\n", argv[0]);
        return 1;
    }
    char* filepath = argv[1];

    // parse file into Graph
    Graph graph = NEW_GRAPH;
    ReturnCode code = parseGraph(filepath, &graph);
    if (code == ERROR) return 1;

    printf("\nSolving '%s'\n", filepath);
    printGraphChars(&graph);

    uint8_t maxWalls = graph.numWalls;

    // iterative deepening on num walls
    // for a fast algorithm, the duplicate work will not matter
    // this is to get a sense of progress on slow algorithms
    for (uint8_t numWalls = 0; numWalls <= maxWalls; ++numWalls)
    {
        graph.numWalls = numWalls;

        clock_t start = clock();

        // solve graph with this number of walls
        Result result = NEW_RESULT;
        solve(&graph, &result);

        double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;

        // print result with this number of walls
        if (result.solutionFound)
        {
            printf("\n");

            // place walls for printing
            for (uint8_t i = 0; i < result.numWalls; ++i)
                graph.nodes[result.walls[i]].type = TILE_WALL;

            // print board
            printGraphChars(&graph);

            // remove walls
            for (uint8_t i = 0; i < result.numWalls; ++i)
                graph.nodes[result.walls[i]].type = TILE_EMPTY;

            // print solve information
            printf("walls: %u -> score: %d (%.3fs)\n", numWalls, result.score, elapsed);
        }
        else printf("\nwalls: %u -> no solution (%.3fs)\n", numWalls, elapsed);

        FREE_RESULT(result);
    }

    FREE_GRAPH(graph);
    return 0;
}