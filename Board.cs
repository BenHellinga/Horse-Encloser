using System.Linq;

namespace HorseEncloser;



public enum Tile
{
    Empty, Horse, Wall, Water, Bee, Cherry, Apple, Portal    
}



public readonly struct TileAttributes(Tile type, bool walkable, bool placeable, int value)
{
    public readonly Tile Type = type;
    public readonly bool Walkable = walkable;
    public readonly bool Placeable = placeable;
    public readonly int Value = value;
}



public record struct Pos(int X, int Y)
{
    public int X = X;
    public int Y = Y;
}



public class Result(int Area, Pos[] walls, Board Board)
{
    public int Area = Area;
    public Pos[] Walls = walls;
    public Board Board = Board;
}



public class Board
{
    // indexed by (int)Tile - order matches the enum:
    // Empty, Horse, Wall, Water, Bee, Cherry, Apple, Portal
    public static readonly TileAttributes[] Attributes =
    [
        new(Tile.Empty,  walkable: true,  placeable: true,  value: 1),
        new(Tile.Horse,  walkable: true,  placeable: false, value: 1),
        new(Tile.Wall,   walkable: false, placeable: false, value: 0),
        new(Tile.Water,  walkable: false, placeable: false, value: 0),
        new(Tile.Bee,    walkable: true,  placeable: false, value: -4),
        new(Tile.Cherry, walkable: true,  placeable: false, value: 4),
        new(Tile.Apple,  walkable: true,  placeable: false, value: 11),
        new(Tile.Portal, walkable: true,  placeable: false, value: 1),
    ];

    public static readonly (int dx, int dy)[] Directions = [(1, 0), (-1, 0), (0, 1), (0, -1)];
    public static readonly int CostlyWallCost = -6;

    public int Width;
    public int Height;
    public Tile[,] Tiles;

    public Pos Start;
    public int NumWalls;
    public Dictionary<Pos, Pos> PortalDestinations;



    // constructor



    public Board((int width, int height) size, (int x, int y) start, int numWalls)
    {
        Width = size.width;
        Height = size.height;
        Start = new Pos(start.x, start.y);
        NumWalls = numWalls;

        Tiles = new Tile[Width, Height];
        Tiles[Start.X, Start.Y] = Tile.Horse;
        PortalDestinations = [];
    }



    // printing



    public static readonly IReadOnlyDictionary<Tile, char> TileChars = new Dictionary<Tile, char>
    {
        { Tile.Empty,   ' ' },
        { Tile.Horse,   'H' },
        { Tile.Wall,    '#' },
        { Tile.Water,   '~' },
        { Tile.Bee,     'B' },
        { Tile.Cherry,  'C' },
        { Tile.Apple,   'A' },
        { Tile.Portal,  'P' }
    };

    private static readonly IReadOnlyDictionary<char, Tile> TilesByChar =
        TileChars.ToDictionary(kv => kv.Value, kv => kv.Key);



    public void Print()
    {
        Console.WriteLine($"+{new string('-', Width * 2 + 1)}+");

        for (int y = Height - 1; y >= 0; --y)
        {
            Console.Write("| ");

            for (int x = 0; x < Width; ++x)
                Console.Write($"{TileChars[Tiles[x, y]]} ");

            Console.Write("|\n");
        }

        Console.WriteLine($"+{new string('-', Width * 2 + 1)}+");
    }



    // save / load



    public void Save(string filepath)
    {
        List<string> lines = [NumWalls.ToString()];

        // same row order as Print(): top of the board (highest Y) first
        for (int y = Height - 1; y >= 0; --y)
        {
            char[] row = new char[Width];
            for (int x = 0; x < Width; ++x)
                row[x] = TileChars[Tiles[x, y]];

            lines.Add(new string(row));
        }

        if (PortalDestinations.Count > 0)
        {
            // the grid alone can't tell two Portal tiles apart, so the links
            // are recorded separately as "x1,y1 x2,y2" pairs after a blank line
            lines.Add(string.Empty);

            HashSet<Pos> written = [];
            foreach ((Pos a, Pos b) in PortalDestinations)
            {
                if (!written.Add(a)) continue; // this pair was already written from the other side
                written.Add(b);
                lines.Add($"{a.X},{a.Y} {b.X},{b.Y}");
            }
        }

        string? dir = Path.GetDirectoryName(filepath);
        if (!string.IsNullOrEmpty(dir))
            Directory.CreateDirectory(dir);

        File.WriteAllLines(filepath, lines);
    }



    public static Board Load(string filepath)
    {
        string[] lines = File.ReadAllLines(filepath);
        if (lines.Length == 0)
            throw new InvalidOperationException($"'{filepath}' is empty.");

        if (!int.TryParse(lines[0], out int numWalls))
            throw new InvalidOperationException($"Expected a wall count on the first line of '{filepath}'.");

        List<string> gridLines = [];
        int i = 1;
        for (; i < lines.Length && lines[i].Length > 0; ++i)
            gridLines.Add(lines[i]);

        if (gridLines.Count == 0)
            throw new InvalidOperationException($"'{filepath}' has no board rows.");

        int width = gridLines[0].Length;
        int height = gridLines.Count;

        Tile[,] tiles = new Tile[width, height];
        Pos start = new(-1, -1);

        for (int row = 0; row < height; ++row)
        {
            string line = gridLines[row];
            if (line.Length != width)
                throw new InvalidOperationException(
                    $"Row {row} of '{filepath}' has length {line.Length}, expected {width}.");

            int y = height - 1 - row; // file rows run top (high Y) to bottom (low Y), matching Save/Print

            for (int x = 0; x < width; ++x)
            {
                char c = line[x];
                if (!TilesByChar.TryGetValue(c, out Tile tile))
                    throw new InvalidOperationException(
                        $"Unrecognized tile character '{c}' at row {row}, col {x} of '{filepath}'.");

                tiles[x, y] = tile;
                if (tile == Tile.Horse)
                    start = new Pos(x, y);
            }
        }

        if (start.X < 0)
            throw new InvalidOperationException($"'{filepath}' has no Horse tile.");

        Board board = new((width, height), (start.X, start.Y), numWalls);
        for (int x = 0; x < width; ++x)
            for (int y = 0; y < height; ++y)
                if (x != start.X || y != start.Y)
                    board.Tiles[x, y] = tiles[x, y];

        // optional portal-link section
        for (int p = i + 1; p < lines.Length; ++p)
        {
            if (lines[p].Length == 0) continue;

            string[] parts = lines[p].Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 2)
                throw new InvalidOperationException($"Malformed portal line '{lines[p]}' in '{filepath}'.");

            Pos a = ParsePos(parts[0], filepath);
            Pos b = ParsePos(parts[1], filepath);

            board.PortalDestinations[a] = b;
            board.PortalDestinations[b] = a;
        }

        return board;
    }



    private static Pos ParsePos(string s, string filepath)
    {
        string[] coords = s.Split(',');
        if (coords.Length != 2 || !int.TryParse(coords[0], out int x) || !int.TryParse(coords[1], out int y))
            throw new InvalidOperationException($"Malformed position '{s}' in '{filepath}'.");

        return new Pos(x, y);
    }



    // helpers



    public Board Copy()
    {
        Board copy = (Board)MemberwiseClone();
        copy.Tiles = (Tile[,])Tiles.Clone();
        copy.PortalDestinations = new Dictionary<Pos, Pos>(PortalDestinations);
        return copy;
    }



    public bool IsEdge(Pos pos) => pos.X == 0 || pos.Y == 0 || pos.X == Width - 1 || pos.Y == Height - 1;



    public bool IsOutside(Pos pos) => pos.X < 0 || pos.Y < 0 || pos.X >= Width || pos.Y >= Height;
}