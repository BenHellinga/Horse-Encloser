# Horse Encloser

**This project is under active development.**

## About

This project automatically solves puzzles from [enclose.horse](https://enclose.horse), a daily puzzle game about enclosing a horse.

In the game, a horse sits on a grid and can move orthogonally, escaping if there's any path leading off the edge of the grid. The player places a limited number of walls on grass tiles to seal the horse into an enclosure. The larger the enclosed area, the higher the score. There are also special tiles that give bonus score, substract score, and allow for teleportation. The goal is to place your limited walls to maximize your final score.

This project attempts to solve that problem automatically: given any puzzle (any grid, any wall count, any layout of special tiles, any gamemode), find the wall placement that produces the highest possible score.

## Implementation

The repo contains four solver versions, each built on the last as bottlenecks were found and better approaches became clear:

- **v1 — Brute force.** Tries every possible combination of wall placements across all valid tiles and keeps the best result. Guaranteed optimal, but scales combinatorially with the number of candidate tiles and the wall count, so it's only practical on small puzzles.

- **v2 — Recursive iterate and block shortest path.** Repeatedly finds the shortest path by which the horse could escape the grid, and branches on blocking each tile along that path with a wall, backtracking through every branch. Once no escape path exists, it optimizes any remaining walls on the interior to maximize score. Still guaranteed optimal, but explores far fewer states than brute force by only considering walls relevant to sealing an escape route, rather than every tile on the board.

- **v3 — Same as v2, with graph optimizations.** Keeps the v2 algorithm but reworks the underlying board representation as a graph, cutting down on repeated work and speeding up the search without changing the approach itself.

- **v4 — Further graph optimizations, plus a new solving algorithm.** Adds more graph level optimizations on top of v3, and introduces a new take on the v2 approach: find the shortest escape path, iterate a blocking wall along that path, and treat everything before the blocker as "inside" and everything after it as "outside." From there it finds the shortest path from inside to outside, pruning branches that are obviously unoptimal by skipping ones that place useless walls.

All four versions are functional. v4 is the current focus of development.

## Usage

Build everything with:

```
make
```

This compiles each `vN_src` directory into its own solver binary (`v1_src/v1`, `v2_src/v2`, etc.) and publishes a self contained Windows build of the C# board reader to `publish-win/`.

Two helper scripts wrap the built binaries:

```
./read <numWalls> <filename>
./solve <filename> [-vN]
```

- `./read <numWalls> <filename>` reads the current puzzle board off the screen (via the published Windows build) and saves it to `boards/<filename>`. This step is Windows only.
- `./solve <filename> [-vN]` runs solver version `N` against a saved board in `boards/<filename>`. `-vN` defaults to `-v4` if omitted.

Board files are plain text: a wall count on the first line, followed by the grid itself (one character per tile), and an optional blank line separated section listing portal tile pairs. Sample boards are included under `boards/`.

## Requirements

Reading the board directly off the screen requires the published Windows build (`make` handles this via `dotnet publish`) and is Windows only. The solvers themselves are plain C and build with `gcc` on any platform.