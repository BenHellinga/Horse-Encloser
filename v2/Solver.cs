namespace HorseEncloser.v2;



public static class Solver
{
    private static Board _board = null!;
    private static List<Pos> _currentWalls = null!;
    private static HashSet<string> _visitedStates = null!;
    private static List<Pos>? _optimizationCandidates = null!;
    private static int? _bestArea = null;
    private static List<Pos> _bestWalls = null!;

    private static bool _costly = false;
    private static bool _verbose = false;
    private static int _reports = -1;
    private static double _step = -1;
    private static double _lastReported = -1;



    public static Result? Solve(Board board, bool costly = false, bool verbose = false, int reports = 20)
    {
        _board = board.Copy();
        _currentWalls = [];
        _visitedStates = [];
        _optimizationCandidates = null;
        _bestArea = null;
        _bestWalls = [];

        _costly = costly;
        _verbose = verbose;
        _reports = reports;
        _step = 1.0 / _reports;
        _lastReported = -1;

        RecursiveBlockShortestPath(_board.NumWalls, 0, 1);
        if (_bestArea == null) return null;

        foreach (Pos wall in _bestWalls)
            _board.Tiles[wall.X, wall.Y] = Tile.Wall;

        return new Result(_bestArea.Value, [.. _bestWalls], _board);
    }



    private static void RecursiveBlockShortestPath(int wallsRemaining, double baseFraction, double scale)
    {
        string key = StateKey(_currentWalls);
        if (!_visitedStates.Add(key)) return;

        (List<Pos>? pathBlockable, List<Pos>? candidates, int score) = FindShortestPathOrOptimizationCandidates(_board);
        _optimizationCandidates = candidates;

        if (pathBlockable == null)
        {
            RecursiveOptimizeInterior(0, wallsRemaining, score);
            return;
        }

        if (wallsRemaining == 0) return;

        int radix = pathBlockable.Count;
        for (int i = radix - 1; i >= 0; i--)
        {
            Pos pos = pathBlockable[i];
            Tile oldTile = _board.Tiles[pos.X, pos.Y];
            _board.Tiles[pos.X, pos.Y] = Tile.Wall;
            _currentWalls.Add(pos);

            double childBase = baseFraction + (radix - 1 - i) / (double)radix * scale;
            double childScale = scale / radix;

            RecursiveBlockShortestPath(wallsRemaining - 1, childBase, childScale);

            _currentWalls.RemoveAt(_currentWalls.Count - 1);
            _board.Tiles[pos.X, pos.Y] = oldTile;

            if (_verbose)
            {
                double percent = (baseFraction + (radix - i) / (double)radix * scale) * 100.0;
                ReportProgress(percent);
            }
        }
    }



    private static string StateKey(List<Pos> currentWalls)
    {
        var sorted = currentWalls.OrderBy(p => p.X).ThenBy(p => p.Y);
        return string.Join(";", sorted.Select(p => $"{p.X},{p.Y}"));
    }



    private static void ReportProgress(double percent)
    {
        double step = 100.0 / _reports;
        double bucket = Math.Floor(percent / step) * step;

        if (bucket > _lastReported)
        {
            _lastReported = bucket;
            Console.WriteLine($"{(int)(percent * 10) / 10.0}%");
        }
    }



    private static void RecursiveOptimizeInterior(int index, int wallsRemaining, int? precomputedScore = null)
    {
        int score = precomputedScore ?? ComputeReachableScore(_board);

        if (_costly)
            score += (_board.NumWalls - wallsRemaining) * Board.CostlyWallCost;

        if (_bestArea == null || score > _bestArea)
        {
            _bestArea = score;
            _bestWalls = [.. _currentWalls];

            if (_verbose)
                Console.WriteLine($"New best: {_bestArea}");
        }

        if (wallsRemaining == 0) return;

        for (int i = index; i < _optimizationCandidates!.Count; i++)
        {
            Pos pos = _optimizationCandidates[i];
            Tile oldTile = _board.Tiles[pos.X, pos.Y];
            _board.Tiles[pos.X, pos.Y] = Tile.Wall;
            _currentWalls.Add(pos);

            RecursiveOptimizeInterior(i + 1, wallsRemaining - 1);

            _currentWalls.RemoveAt(_currentWalls.Count - 1);
            _board.Tiles[pos.X, pos.Y] = oldTile;
        }
    }



    private static (List<Pos>? path, List<Pos>? candidates, int score) FindShortestPathOrOptimizationCandidates(Board board)
    {
        int w = board.Width;
        int h = board.Height;

        bool[,] visited = new bool[w, h];
        Pos[,] cameFrom = new Pos[w, h];
        Queue<Pos> queue = new();
        List<Pos> candidates = [];

        int score = 1;
        Pos start = board.Start;
        visited[start.X, start.Y] = true;
        queue.Enqueue(start);

        while (queue.Count > 0)
        {
            Pos current = queue.Dequeue();
            Pos next;
            Tile tile;
            int points;

            foreach (var (dx, dy) in Board.Directions)
            {
                int nx = current.X + dx;
                int ny = current.Y + dy;
                next = new(nx, ny);

                if (board.IsOutside(next)) continue;

                tile = board.Tiles[nx, ny];

                TileAttributes attrs = Board.Attributes[(int)tile];
                if (!attrs.Walkable) continue;
                points = attrs.Value;
                if (visited[next.X, next.Y]) continue;

                var result = VisitTile();
                if (result != null) return (result.Value.Item1, result.Value.Item2, 0);
            }

            if (board.Tiles[current.X, current.Y] == Tile.Portal)
            {
                next = board.PortalDestinations[current];
                tile = board.Tiles[next.X, next.Y];

                TileAttributes attrs = Board.Attributes[(int)tile];
                if (attrs.Walkable && !visited[next.X, next.Y])
                {
                    points = attrs.Value;
                    var result = VisitTile();
                    if (result != null) return (result.Value.Item1, result.Value.Item2, 0);
                }
            }

            (List<Pos>?, List<Pos>?)? VisitTile()
            {
                cameFrom[next.X, next.Y] = current;

                if (board.IsEdge(next))
                {
                    List<Pos> path = ReconstructPath(cameFrom, start, next);
                    return (path, null);
                }

                score += points;
                visited[next.X, next.Y] = true;
                queue.Enqueue(next);

                if (Board.Attributes[(int)tile].Placeable)
                    candidates.Add(next);

                return null;
            }
        }

        return (null, candidates, score);
    }



    private static int ComputeReachableScore(Board board)
    {
        int w = board.Width;
        int h = board.Height;

        bool[,] visited = new bool[w, h];
        Queue<Pos> queue = new();

        int score = 1;
        Pos start = board.Start;
        visited[start.X, start.Y] = true;
        queue.Enqueue(start);

        while (queue.Count > 0)
        {
            Pos current = queue.Dequeue();
            Pos next;
            int points;

            foreach (var (dx, dy) in Board.Directions)
            {
                int nx = current.X + dx;
                int ny = current.Y + dy;
                next = new(nx, ny);

                if (board.IsOutside(next)) continue;

                Tile tile = board.Tiles[nx, ny];

                TileAttributes attrs = Board.Attributes[(int)tile];
                if (!attrs.Walkable) continue;
                points = attrs.Value;
                if (visited[next.X, next.Y]) continue;

                VisitTile();
            }

            if (board.Tiles[current.X, current.Y] == Tile.Portal)
            {
                next = board.PortalDestinations[current];

                TileAttributes attrs = Board.Attributes[(int)board.Tiles[next.X, next.Y]];
                if (attrs.Walkable && !visited[next.X, next.Y])
                {
                    points = attrs.Value;
                    VisitTile();
                }
            }

            void VisitTile()
            {
                score += points;
                visited[next.X, next.Y] = true;
                queue.Enqueue(next);
            }
        }

        return score;
    }



    private static List<Pos> ReconstructPath(Pos[,] cameFrom, Pos start, Pos end)
    {
        List<Pos> path = [end];
        Pos current = end;

        while (current.X != start.X || current.Y != start.Y)
        {
            current = cameFrom[current.X, current.Y];

            Tile tile = _board.Tiles[current.X, current.Y];
            if (Board.Attributes[(int)tile].Placeable)
                path.Add(current);
        }

        path.Reverse();
        return path;
    }
}