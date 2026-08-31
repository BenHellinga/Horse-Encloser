using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace HorseEncloser;



public enum Tile
{
    Empty, Horse, Unicorn, Wall, Water, Bee, Cherry, Apple, BluePortal, PinkPortal, PurplePortal
}



public enum Gamemode
{
    Classic, Costly, Lovebirds, Quarrel
}



/*
   Reads a board off the screen by template-matching each tile, then
   saves it to a text file and prints it.
*/



[SupportedOSPlatform("windows")]
public static class Program
{
    public static void Main(string[] args)
    {
        if (args.Length != 4 || args[0] != "-w" || args[2] != "-s")
        {
            Console.WriteLine("Usage: -w <numWalls> -s <filepath>");
            return;
        }
 
        if (!int.TryParse(args[1], out int numWalls))
        {
            Console.WriteLine($"'-w' expects an integer, got '{args[1]}'.");
            return;
        }
 
        WebsiteReader.ReadSaveAndPrint(numWalls, args[3]);
    }
}



[SupportedOSPlatform("windows")]
public static class WebsiteReader
{
    // tile <-> character mapping, used for saving and printing

    private static readonly IReadOnlyDictionary<Tile, char> TileChars = new Dictionary<Tile, char>
    {
        { Tile.Empty,        ' ' },
        { Tile.Horse,        'H' },
        { Tile.Unicorn,      'U' },
        { Tile.Wall,         '#' },
        { Tile.Water,        '~' },
        { Tile.Bee,          'B' },
        { Tile.Cherry,       'C' },
        { Tile.Apple,        'A' },
        { Tile.BluePortal,   '1' },
        { Tile.PinkPortal,   '2' },
        { Tile.PurplePortal, '3' },
    };



    // win32 interop



    [DllImport("user32.dll")]
    private static extern bool GetCursorPos(out POINT lpPoint);

    [DllImport("user32.dll")]
    private static extern int GetSystemMetrics(int nIndex);

    [DllImport("user32.dll")]
    private static extern bool SetProcessDPIAware();

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT
    {
        public int X;
        public int Y;
    }

    private const int SM_XVIRTUALSCREEN = 76;
    private const int SM_YVIRTUALSCREEN = 77;
    private const int SM_CXVIRTUALSCREEN = 78;
    private const int SM_CYVIRTUALSCREEN = 79;



    // colors



    private static readonly HashSet<(byte R, byte G, byte B)> BackgroundColors =
    [
        HexToRgb(0x1b6b3a), // classic background green a
        // HexToRgb(0x1e8a48), // classic background green b
        HexToRgb(0x802213), // costly background red
        HexToRgb(0x272727), // quarrel background gray a
        // HexToRgb(0x424242), // quarrel background gray b
        HexToRgb(0x9c78b1), // lovebirds background pink a
        // HexToRgb(0xa580b9), // lovebirds background pink b
    ];

    private static (byte R, byte G, byte B) HexToRgb(int hex) =>
        ((byte)((hex >> 16) & 0xFF), (byte)((hex >> 8) & 0xFF), (byte)(hex & 0xFF));



    // template sampling



    private const int TemplateSize = 4;
    private const double TemplatePadding = 0.3;
    private const double ColorThreshold = 0.05;
    private const double TemplateThreshold = 0.1;
    private const string TemplateFolder = "templates";
    private const string DebugFolder = "debug";



    // public api



    public static void ReadSaveAndPrint(int numWalls, string filepath)
    {
        (Tile[,]? tiles, Gamemode gamemode) = ReadBoardFromScreen();
        if (tiles == null) return;

        Save(tiles, gamemode, numWalls, filepath);
        Print(tiles);
        Console.WriteLine($"Gamemode: {gamemode}");
        Console.WriteLine($"Walls: {numWalls}");
        Console.WriteLine($"Width/Height: ({tiles.GetLength(0)}, {tiles.GetLength(1)})");
    }



    private static (Tile[,]? tiles, Gamemode gamemode) ReadBoardFromScreen()
    {
        SetProcessDPIAware();

        Console.WriteLine("Position the cursor to the left of the board, then press ENTER");
        Console.ReadLine();

        if (!GetCursorPos(out POINT cursor))
        {
            Console.WriteLine("Could not read cursor position");
            return (null, Gamemode.Classic);
        }

        (Bitmap screenBmp, int originX, int originY) = CaptureScreen();

        FastBitmap screenFb = new(screenBmp);
        int cursorX = cursor.X - originX;
        int cursorY = cursor.Y - originY;

        // scan right from the cursor (on the green background) until we hit the board itself
        int xLeft = ScanRightUntilFirstNonGreen(screenFb, cursorX, cursorY, screenFb.Width);
        if (xLeft < 0) return (null, Gamemode.Classic);

        // scan up/down and then right to find the other three edges of the board
        (int yTop, int yBottom) = ScanUpAndDownUntilFirstGreen(screenFb, xLeft, cursorY);
        int xRight = ScanRightUntilFirstGreen(screenFb, xLeft, yTop);

        int boardW = xRight - xLeft + 1;
        int boardH = yBottom - yTop + 1;
        using Bitmap boardBmp = screenBmp.Clone(new Rectangle(xLeft, yTop, boardW, boardH), PixelFormat.Format24bppRgb);
        using FastBitmap boardFb = new(boardBmp);

        // scan diagonally from the top-left corner to find the border thickness,
        // then scan the first inner tile to find the tile size
        int borderThickness = FindBorderThickness(boardFb);
        if (borderThickness < 0) return (null, Gamemode.Classic);

        int tileSize = FindTileSize(boardFb, borderThickness);
        (int cols, int rows) = FindBoardDimensions(boardW, boardH, tileSize, borderThickness);

        double periodX = (boardW - borderThickness) / (double)cols;
        double periodY = (boardH - borderThickness) / (double)rows;

        List<Template> templates = LoadTemplates();
        (Tile[,]? tiles, Gamemode gamemode) = ClassifyTiles(boardBmp, cols, rows, borderThickness, periodX, periodY, templates);
        foreach (Template t in templates) t.Image.Dispose();

        return (tiles, gamemode);
    }



    // bitmaps



    private static (Bitmap bmp, int originX, int originY) CaptureScreen()
    {
        int originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int originY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        Bitmap bmp = new(width, height, PixelFormat.Format24bppRgb);
        using (Graphics g = Graphics.FromImage(bmp))
            g.CopyFromScreen(originX, originY, 0, 0, new Size(width, height));

        return (bmp, originX, originY);
    }



    // scanning



    private static double ColorDistance((byte R, byte G, byte B) a, (byte R, byte G, byte B) b)
    {
        double dr = a.R - b.R;
        double dg = a.G - b.G;
        double db = a.B - b.B;
        return Math.Sqrt(dr * dr + dg * dg + db * db) / 0xff;
    }



    private static bool IsBackground((byte R, byte G, byte B) c) =>
        BackgroundColors.Any(bg => ColorDistance(c, bg) <= ColorThreshold);



    private static int ScanRightUntilFirstNonGreen(FastBitmap fb, int startX, int y, int maxX)
    {
        if (y < 0 || y >= fb.Height || startX < 0)
        {
            Console.WriteLine("Right scan failed");
            return -1;
        }

        for (int x = startX; x < maxX; ++x)
            if (!IsBackground(fb.GetPixel(x, y)))
                return x;

        Console.WriteLine("Right scan failed");
        return -1;
    }



    private static (int top, int bottom) ScanUpAndDownUntilFirstGreen(FastBitmap fb, int x, int startY)
    {
        int top = startY;
        while (top - 1 >= 0 && !IsBackground(fb.GetPixel(x, top - 1))) --top;

        int bottom = startY;
        while (bottom + 1 < fb.Height && !IsBackground(fb.GetPixel(x, bottom + 1))) ++bottom;

        return (top, bottom);
    }



    private static int ScanRightUntilFirstGreen(FastBitmap fb, int startX, int y)
    {
        int x = startX;
        while (x + 1 < fb.Width && !IsBackground(fb.GetPixel(x + 1, y))) ++x;

        return x;
    }



    // board geometry



    private static int FindBorderThickness(FastBitmap boardFb)
    {
        int t = 0;
        int maxT = Math.Min(boardFb.Width, boardFb.Height) - 1;
        while (t < maxT && !IsBackground(boardFb.GetPixel(t, t))) ++t;

        if (t >= maxT)
        {
            Console.WriteLine("Diagonal scan failed");
            return -1;
        }

        return t;
    }



    private static int FindTileSize(FastBitmap boardFb, int borderThickness)
    {
        int x = borderThickness;
        while (x + 1 < boardFb.Width && IsBackground(boardFb.GetPixel(x + 1, borderThickness))) ++x;

        return x - borderThickness + 1;
    }



    private static (int cols, int rows) FindBoardDimensions(int boardWidth, int boardHeight, int tileSize, int borderThickness)
    {
        double period = tileSize + borderThickness;
        int cols = (int)Math.Round((boardWidth - borderThickness) / period);
        int rows = (int)Math.Round((boardHeight - borderThickness) / period);

        return (cols, rows);
    }



    // returns the pixel bounds of the inner (padded) region of tile (gx, gy),
    // i.e. the part of the tile we actually screenshot and compare to templates
    private static (int x0, int y0, int w, int h) GetPaddedTileBoundsPx(int gx, int gy, int borderThickness, double periodX, double periodY)
    {
        int x0 = (int)Math.Round(borderThickness + gx * periodX);
        int y0 = (int)Math.Round(borderThickness + gy * periodY);
        int x1 = (int)Math.Round(borderThickness + (gx + 1) * periodX);
        int y1 = (int)Math.Round(borderThickness + (gy + 1) * periodY);

        int w = x1 - x0;
        int h = y1 - y0;
        int padX = (int)Math.Round(w * TemplatePadding / 2.0);
        int padY = (int)Math.Round(h * TemplatePadding / 2.0);

        return (x0 + padX, y0 + padY, w - 2 * padX, h - 2 * padY);
    }



    // cropping / resizing



    private static Bitmap ExtractTileCrop(Bitmap src, int cellX0, int cellY0, int cellW, int cellH)
    {
        int x = Math.Max(0, cellX0);
        int y = Math.Max(0, cellY0);
        int w = Math.Max(1, Math.Min(cellW, src.Width - x));
        int h = Math.Max(1, Math.Min(cellH, src.Height - y));

        return src.Clone(new Rectangle(x, y, w, h), PixelFormat.Format24bppRgb);
    }



    private static Bitmap ResizeTo(Bitmap src, int w, int h)
    {
        Bitmap dst = new(w, h, PixelFormat.Format24bppRgb);
        using Graphics g = Graphics.FromImage(dst);
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.DrawImage(src, 0, 0, w, h);
        return dst;
    }



    // templates + classification



    // Gamemode is null for templates shared across gamemodes (e.g. "empty_0"),
    // and set for templates specific to one gamemode (e.g. "empty_costly_0")
    private sealed class Template
    {
        public required Tile Tile;
        public required Gamemode? Gamemode;
        public required string Name;
        public required FastBitmap Image;
    }



    private static List<Template> LoadTemplates()
    {
        List<Template> list = [];

        foreach (string file in Directory.GetFiles(TemplateFolder))
        {
            string ext = Path.GetExtension(file).ToLowerInvariant();
            if (ext is not (".png" or ".bmp" or ".jpg" or ".jpeg")) continue;

            // filenames look like "{tile}_{n}.png" (shared across gamemodes) or
            // "{tile}_{gamemode}_{n}.png" (specific to one gamemode), e.g.
            // "Wall1.png", "Empty_0.png", "Empty_Costly_0.png"
            string name = Path.GetFileNameWithoutExtension(file);
            string[] tokens = name.Split(['_', '-', ' '], StringSplitOptions.RemoveEmptyEntries);

            string prefix = tokens[0];

            if (!Enum.TryParse(prefix, ignoreCase: true, out Tile tile))
            {
                Console.WriteLine($"Warning: Couldn't parse a Tile type from '{name}', skipping.");
                continue;
            }

            Gamemode? gamemode = null;

            if (tokens.Length >= 3)
            {
                if (!Enum.TryParse(tokens[1], ignoreCase: true, out Gamemode parsedGamemode))
                {
                    Console.WriteLine($"Warning: Couldn't parse a Gamemode from '{name}', skipping.");
                    continue;
                }

                gamemode = parsedGamemode;
            }

            using Bitmap raw = new(file);
            using Bitmap resized = ResizeTo(raw, TemplateSize, TemplateSize);
            list.Add(new Template { Tile = tile, Gamemode = gamemode, Name = name, Image = new FastBitmap(resized) });
        }

        return list;
    }



    private static (Tile[,]? tiles, Gamemode gamemode) ClassifyTiles(
        Bitmap boardBmp, int cols, int rows, int borderThickness, double periodX, double periodY,
        List<Template> templates)
    {
        Tile[,] tiles = new Tile[cols, rows];
        bool anyUnrecognized = false;
        Gamemode? detectedGamemode = null;

        for (int gx = 0; gx < cols; gx++)
        {
            for (int gy = 0; gy < rows; gy++)
            {
                (int cellX0, int cellY0, int cellW, int cellH) = GetPaddedTileBoundsPx(gx, gy, borderThickness, periodX, periodY);

                using Bitmap tileCrop = ExtractTileCrop(boardBmp, cellX0, cellY0, cellW, cellH);
                using Bitmap resized = ResizeTo(tileCrop, TemplateSize, TemplateSize);
                using FastBitmap tileFb = new(resized);

                (Template? best, double score) = ClassifyTile(tileFb, templates);

                if (best is null || score > TemplateThreshold)
                {
                    Console.WriteLine($"Warning: no confident match at cell ({gx},{gy}) (closest {best?.Tile} @ {score:F3})");
                    SaveDebugTile(resized);
                    anyUnrecognized = true;
                    continue;
                }

                // a template specific to one gamemode identifies the board's gamemode;
                // shared templates (Gamemode == null) don't tell us anything
                if (best.Gamemode.HasValue)
                {
                    if (detectedGamemode is null)
                        detectedGamemode = best.Gamemode.Value;
                    else if (detectedGamemode.Value != best.Gamemode.Value)
                        Console.WriteLine($"Warning: cell ({gx},{gy}) matched '{best.Name}' ({best.Gamemode}), conflicting with already-detected gamemode {detectedGamemode}.");
                }

                int boardX = gx;
                int boardY = rows - 1 - gy; // screen top row -> highest board Y (matches Print/Save)
                tiles[boardX, boardY] = best.Tile;
            }
        }

        if (anyUnrecognized)
        {
            Console.WriteLine("Error: One or more tiles could not be classified with confidence.");
            return (null, Gamemode.Classic);
        }

        // no gamemode-specific tile was ever matched, so the board is classic
        return (tiles, detectedGamemode ?? Gamemode.Classic);
    }



    private static (Template? best, double score) ClassifyTile(FastBitmap cell, List<Template> templates)
    {
        Template? best = null;
        double bestScore = double.MaxValue;

        foreach (Template t in templates)
        {
            double score = ComputeMatchScore(cell, t.Image);
            if (score < bestScore)
            {
                bestScore = score;
                best = t;
            }
        }

        return (best, bestScore);
    }



    private static double ComputeMatchScore(FastBitmap a, FastBitmap b)
    {
        double totalDistance = 0;
        int total = a.Width * a.Height;

        for (int y = 0; y < a.Height; y++)
            for (int x = 0; x < a.Width; x++)
                totalDistance += ColorDistance(a.GetPixel(x, y), b.GetPixel(x, y));

        return totalDistance / total;
    }



    // debug output



    // lazily initialized on first use; -1 means "not yet scanned"
    private static int _debugTileCounter = -1;

    private static void SaveDebugTile(Bitmap tileBmp)
    {
        // works whether or not the debug folder already exists
        Directory.CreateDirectory(DebugFolder);

        if (_debugTileCounter < 0)
        {
            _debugTileCounter = 0;
            foreach (string f in Directory.GetFiles(DebugFolder, "*.png"))
            {
                if (int.TryParse(Path.GetFileNameWithoutExtension(f), out int n) && n >= _debugTileCounter)
                    _debugTileCounter = n + 1;
            }
        }

        tileBmp.Save(Path.Combine(DebugFolder, $"{_debugTileCounter}.png"), ImageFormat.Png);
        _debugTileCounter++;
    }



    // printing



    private static void Print(Tile[,] tiles)
    {
        int width = tiles.GetLength(0);
        int height = tiles.GetLength(1);

        Console.WriteLine($"+{new string('-', width * 2 + 1)}+");

        for (int y = height - 1; y >= 0; --y)
        {
            Console.Write("| ");

            for (int x = 0; x < width; ++x)
                Console.Write($"{TileChars[tiles[x, y]]} ");

            Console.Write("|\n");
        }

        Console.WriteLine($"+{new string('-', width * 2 + 1)}+");
    }



    // saving



    private static void Save(Tile[,] tiles, Gamemode gamemode, int numWalls, string filepath)
    {
        int width = tiles.GetLength(0);
        int height = tiles.GetLength(1);

        List<string> lines =
        [
            "1",
            gamemode.ToString().ToLowerInvariant(),
            $"{numWalls}, {width}, {height}",
        ];

        // same row order as Print(): top of the board (highest Y) first.
        // no separate portal-link section is needed: each portal color
        // appears on exactly two tiles, so the color alone identifies the pair.
        for (int y = height - 1; y >= 0; --y)
        {
            char[] row = new char[width];
            for (int x = 0; x < width; ++x)
                row[x] = TileChars[tiles[x, y]];

            lines.Add(new string(row));
        }

        string? dir = Path.GetDirectoryName(filepath);
        if (!string.IsNullOrEmpty(dir))
            Directory.CreateDirectory(dir);

        File.WriteAllLines(filepath, lines);
    }
}



// minimal locked-bitmap wrapper for fast pixel reads



[SupportedOSPlatform("windows")]
internal sealed class FastBitmap : IDisposable
{
    public int Width { get; }
    public int Height { get; }
    private readonly int _stride;
    private readonly byte[] _buffer;



    public FastBitmap(Bitmap bmp)
    {
        Width = bmp.Width;
        Height = bmp.Height;

        BitmapData data = bmp.LockBits(
            new Rectangle(0, 0, Width, Height),
            ImageLockMode.ReadOnly,
            PixelFormat.Format24bppRgb);

        _stride = data.Stride;
        _buffer = new byte[_stride * Height];
        Marshal.Copy(data.Scan0, _buffer, 0, _buffer.Length);
        bmp.UnlockBits(data);
    }



    public (byte R, byte G, byte B) GetPixel(int x, int y)
    {
        int idx = y * _stride + x * 3;
        return (_buffer[idx + 2], _buffer[idx + 1], _buffer[idx]);
    }



    public void Dispose() {}
}