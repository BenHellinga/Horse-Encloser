namespace HorseEncloser.v1;



public static class Solver
{
    private static Board _board = null!;
    private static List<Pos> _candidates = null!;
    private static List<Pos> _currentWalls = null!;
    private static int? _bestArea = null;
    private static List<Pos> _bestWalls = null!;

    private static bool _costly = false;
    private static bool _verbose = false;
    private static int _reports = -1;
    private static double _step = -1;
    private static double _lastReported = -1;
    private static long _total = -1;
    private static long _completed = -1;



    public static Result? Solve(Board board, bool costly = false, bool verbose = false, int reports = 20)
    {
        _board = board.Copy();
        _candidates = [];
        _currentWalls = [];
        _bestArea = null;
        _bestWalls = [];

        for (int x = 0; x < board.Width; x++)
            for (int y = 0; y < board.Height; y++)
                if (Board.Attributes[(int)board.Tiles[x, y]].Placeable)
                    _candidates.Add(new Pos(x, y));

        _costly = costly;
        _verbose = verbose;
        _reports = reports;
        _step = 1.0 / _reports;
        _lastReported = -1;
        _total = BinomialCoefficient(_candidates.Count, board.NumWalls);
        _completed = 0;

        RecursiveBruteForceWalls(0, _board.NumWalls);
        if (_bestArea is null) return null;

        foreach (Pos wall in _bestWalls)
            _board.Tiles[wall.X, wall.Y] = Tile.Wall;

        return new Result(_bestArea.Value, [.. _bestWalls], _board);
    }



    private static void RecursiveBruteForceWalls(int index, int wallsRemaining)
    {
        int wallsUsed = _board.NumWalls - wallsRemaining;

        int? area = ComputeEnclosedArea(_board);
        if (area != null)
        {
            if (_costly)
                area += wallsUsed * Board.CostlyWallCost;

            if (_bestArea == null || area > _bestArea)
            {
                _bestArea = area;
                _bestWalls.Clear();
                _bestWalls.AddRange(_currentWalls);

                if (_verbose)
                    Console.WriteLine($"New Best: {_bestArea}");
            }
        }

        if (wallsRemaining == 0) return;

        int lastIndex = _candidates.Count - wallsRemaining;
        for (int i = index; i <= lastIndex; i++)
        {
            Pos pos = _candidates[i];
            Tile oldTile = _board.Tiles[pos.X, pos.Y];
            _board.Tiles[pos.X, pos.Y] = Tile.Wall;
            _currentWalls.Add(pos);

            RecursiveBruteForceWalls(i + 1, wallsRemaining - 1);

            _currentWalls.RemoveAt(_currentWalls.Count - 1);
            _board.Tiles[pos.X, pos.Y] = oldTile;

            if (_verbose && wallsUsed == 0)
            {
                _completed += BinomialCoefficient(_candidates.Count - i - 1, wallsRemaining - 1);
                double percent = _completed / (double)_total * 1.0;
                double bucket = Math.Floor(percent / _step) * _step;

                if (bucket > _lastReported)
                {
                    _lastReported = bucket;
                    Console.WriteLine($"{percent * 100:0.00}%");
                }
            }
        }
    }



    private static long BinomialCoefficient(int n, int k)
    {
        if (k < 0 || k > n) return 0;
        k = Math.Min(k, n - k);
        
        long result = 1;
        for (int i = 0; i < k; i++)
            result = result * (n - i) / (i + 1);

        return result;
    }



    private static int? ComputeEnclosedArea(Board board)
    {
        int w = board.Width;
        int h = board.Height;

        int num;
        bool[,] visited = new bool[w, h];
        Queue<Pos> queue = new();

        num = 1;
        visited[board.Start.X, board.Start.Y] = true;
        queue.Enqueue(board.Start);

        while (queue.Count > 0)
        {
            Pos current = queue.Dequeue();

            foreach (var (dx, dy) in Board.Directions)
            {
                int nx = current.X + dx;
                int ny = current.Y + dy;
                Pos next = new(nx, ny);

                if (board.IsOutside(next)) continue;

                Tile tile = board.Tiles[nx, ny];

                TileAttributes attrs = Board.Attributes[(int)tile];
                if (!attrs.Walkable) continue;
                int points = attrs.Value;
                if (visited[next.X, next.Y]) continue;
                if (board.IsEdge(next)) return null;

                num += points;
                visited[next.X, next.Y] = true;
                queue.Enqueue(next);
            }

            if (board.Tiles[current.X, current.Y] == Tile.Portal)
            {
                Pos dest = board.PortalDestinations[current];
                if (!visited[dest.X, dest.Y])
                {
                    num += Board.Attributes[(int)Tile.Portal].Value;
                    visited[dest.X, dest.Y] = true;
                    queue.Enqueue(dest);
                }
            }
        }

        return num;
    }
}