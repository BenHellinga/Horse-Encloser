# Horse Encloser

**This project is under active development.**

## About

This project automatically solves puzzles from [enclose.horse](https://enclose.horse), a daily puzzle game about enclosing a horse.

In the game, a horse sits on a grid and can move up, down, left, or right (never diagonally), escaping if there's any gap leading off the edge of the grid. The player places a limited number of walls on grass tiles to seal the horse into an enclosure. The larger the enclosed area, the higher the score — and some tiles inside the enclosure add or subtract from that score directly: cherries and golden apples are worth points, bee swarms cost points, and portals let the horse teleport between two linked tiles. The goal each puzzle is to place your limited walls to maximize your final score.

This project attempts to solve that problem automatically — given any puzzle (any grid, any wall count, any layout of special tiles) and any of the game's scoring rule variants, find the wall placement that produces the highest possible score.

## Development

The project is still evolving, and the repo currently contains three solver approaches, developed in order as the previous ones proved too slow or as a better approach became clear:

- **v1 — Brute force.** Tries every possible combination of wall placements across all valid tiles and keeps the best result. Guaranteed optimal, but scales combinatorially with the number of candidate tiles and the wall count, so it's only practical on small puzzles.

- **v2 — Recursive iterate-and-block-shortest-path.** Repeatedly finds the shortest path by which the horse could escape the grid, and branches on blocking each tile along that path with a wall, backtracking through every branch. Once no escape path exists, it optimizes any remaining walls on the interior to maximize score. This is still guaranteed to find the optimal solution, but explores dramatically fewer states than brute force by only considering walls that are actually relevant to sealing an escape route, rather than every tile on the board.

- **v3 — Divide and conquer (in progress).** The direction being explored now: split the board into sub-regions, solve each sub-region for its own optimal enclosure, and merge those partial solutions into a global optimum. The goal is to scale to larger puzzles than v2 can handle in reasonable time. This version is still being designed and isn't functional yet — the current code is scaffolding, not a working solver.

v1 and v2 both work and produce correct, optimal solutions. v3 is the active area of development.

Once v3 is finalized, the plan is to rewrite the project in C, largely overhauling the codebase in the process — the current C# implementation was chosen for speed of development while iterating on the solving approach, not as the long-term implementation.

## Usage

```
HorseEncloser.exe -w <numWalls>
HorseEncloser.exe -w <numWalls> -s <filepath>
HorseEncloser.exe -l <filepath>
```

- `-w <numWalls>` — reads the current puzzle board (via `WebsiteIO`) and solves it with the given wall count.
- `-s <filepath>` — optionally saves the board read from the site to a file.
- `-l <filepath>` — loads a previously saved board from a file instead of reading from the site.

`-w`/`-s` and `-l` are mutually exclusive; `-s` is only valid alongside `-w`.

Board files are plain text: a wall count on the first line, followed by the grid itself (one character per tile), and an optional blank-line-separated section listing portal tile pairs.

## Requirements

Reading the board directly from the website (`WebsiteIO`) is Windows-only.