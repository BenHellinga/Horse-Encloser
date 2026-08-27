using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace HorseEncloser;



[SupportedOSPlatform("windows")]
public static class WebsiteIO
{
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



    private static readonly (byte R, byte G, byte B) GreenA = HexToRgb(0x1e8a48);
    private static readonly (byte R, byte G, byte B) GreenB = HexToRgb(0x1b6b3a);

    private static (byte R, byte G, byte B) HexToRgb(int hex) => ((byte)((hex >> 16) & 0xFF), (byte)((hex >> 8) & 0xFF), (byte)(hex & 0xFF));



    // template sampling



    private const int TemplateSize = 4;
    private const double TemplatePadding = 0.3;
    private const double ColorThreshold = 0.05;
    private const double TemplateThreshold = 0.1;
    private const string TemplateFolder = "templates";



    // public api



    public static Board? ReadBoardFromScreen(int numWalls)
    {
        SetProcessDPIAware();

        Console.WriteLine("Position the cursor to the left of the board, then press ENTER");
        Console.ReadLine();

        if (!GetCursorPos(out POINT cursor))
        {
            Console.WriteLine("Could not read cursor position");
            return null;
        }

        (Bitmap screenBmp, int originX, int originY) = CaptureScreen();

        FastBitmap screenFb = new(screenBmp);
        int cursorX = cursor.X - originX;
        int cursorY = cursor.Y - originY;

        int xLeft = ScanRightUntilFirstNonGreen(screenFb, cursorX, cursorY, screenFb.Width);
        if (xLeft < 0) return null;

        (int yTop, int yBottom) = ScanUpAndDownUntilFirstGreen(screenFb, xLeft, cursorY);
        int xRight = ScanRightUntilFirstGreen(screenFb, xLeft, yTop);

        // crop bitmap now that board is located
        int boardW = xRight - xLeft + 1;
        int boardH = yBottom - yTop + 1;
        using Bitmap boardBmp = screenBmp.Clone(new Rectangle(xLeft, yTop, boardW, boardH), PixelFormat.Format24bppRgb);
        using FastBitmap boardFb = new(boardBmp);

        int borderThickness = FindBorderThickness(boardFb);
        if (borderThickness < 0) return null;

        int tileSizeEstimate = FindTileSize(boardFb, borderThickness);


        (int cols, int rows) = FindBoardDimensions(boardW, boardH, tileSizeEstimate, borderThickness);

        double periodX = (boardW - borderThickness) / (double)cols;
        double periodY = (boardH - borderThickness) / (double)rows;

        SaveDebugGridOverlay(boardBmp, cols, rows, borderThickness, periodX, periodY);

        List<Template> templates = LoadTemplates();
        (Tile[,], Pos?, Dictionary<int, List<Pos>>)? result = ClassifyTiles(boardBmp, cols, rows, borderThickness, periodX, periodY, templates);
        foreach (Template tmpl in templates) tmpl.Image.Dispose();
        if (result == null) return null;

        (Tile[,] tiles, Pos? start, Dictionary<int, List<Pos>> portalGroups) = result.Value;

        if (start == null)
        {
            Console.WriteLine("No horse found");
            return null;
        }

        Board board = new((cols, rows), (start.Value.X, start.Value.Y), numWalls);

        for (int x = 0; x < cols; x++)
            for (int y = 0; y < rows; y++)
                board.Tiles[x, y] = tiles[x, y];

        foreach ((int _, List<Pos> pos) in portalGroups)
        {
            board.PortalDestinations.Add(pos[0], pos[1]);
            board.PortalDestinations.Add(pos[1], pos[0]);
        }

        return board;
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



    private static void SaveDebugGridOverlay(Bitmap boardBmp, int cols, int rows, int borderThickness, double periodX, double periodY)
    {
        using FastBitmap overlayFb = new(boardBmp);

        for (int x = 0; x < cols; x++)
        {
            for (int y = 0; y < rows; y++)
            {
                (int cellX0, int cellY0, _, _) = GetTileBoundsPx(x, y, borderThickness, periodX, periodY);
                overlayFb.SetPixelWhite(cellX0, cellY0);
            }
        }

        overlayFb.Save("debug/board.png");
    }



    // scanning



    private static double ColorDistance((byte R, byte G, byte B) a, (byte R, byte G, byte B) b)
    {
        double dr = a.R - b.R;
        double dg = a.G - b.G;
        double db = a.B - b.B;
        return Math.Sqrt(dr * dr + dg * dg + db * db) / 0xff;
    }



    private static bool IsGreenish((byte R, byte G, byte B) c) =>
        ColorDistance(c, GreenA) <= ColorThreshold || ColorDistance(c, GreenB) <= ColorThreshold;



    private static int ScanRightUntilFirstNonGreen(FastBitmap fb, int startX, int y, int maxX)
    {
        if (y < 0 || y >= fb.Height || startX < 0)
        {
            Console.WriteLine("Right scan failed");
            return -1;
        }

        for (int x = startX; x < maxX; ++x)
            if (!IsGreenish(fb.GetPixel(x, y)))
                return x;

        Console.WriteLine("Right scan failed");
        return -1;
    }



    private static (int top, int bottom) ScanUpAndDownUntilFirstGreen(FastBitmap fb, int x, int startY)
    {
        int top = startY;
        while (top - 1 >= 0 && !IsGreenish(fb.GetPixel(x, top - 1))) --top;

        int bottom = startY;
        while (bottom + 1 < fb.Height && !IsGreenish(fb.GetPixel(x, bottom + 1))) ++bottom;

        return (top, bottom);
    }



    private static int ScanRightUntilFirstGreen(FastBitmap fb, int startX, int y)
    {
        int x = startX;
        while (x + 1 < fb.Width && !IsGreenish(fb.GetPixel(x + 1, y))) ++x;

        return x;
    }



    // board geometry



    private static int FindBorderThickness(FastBitmap boardFb)
    {
        int t = 0;
        int maxT = Math.Min(boardFb.Width, boardFb.Height) - 1;
        while (t < maxT && !IsGreenish(boardFb.GetPixel(t, t))) ++t;

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
        while (x + 1 < boardFb.Width && IsGreenish(boardFb.GetPixel(x + 1, borderThickness))) ++x;

        return x - borderThickness + 1;
    }



    private static (int cols, int rows) FindBoardDimensions(int boardWidth, int boardHeight, int tileSize, int borderThickness)
    {
        double period = tileSize + borderThickness;
        int cols = (int)Math.Round((boardWidth - borderThickness) / period);
        int rows = (int)Math.Round((boardHeight - borderThickness) / period);

        return (cols, rows);
    }



    private static (int x, int y, int w, int h) GetTileBoundsPx(int gx, int gy, int borderThickness, double periodX, double periodY)
    {
        int x0 = (int)Math.Round(borderThickness + gx * periodX);
        int y0 = (int)Math.Round(borderThickness + gy * periodY);
        int x1 = (int)Math.Round(borderThickness + (gx + 1) * periodX);
        int y1 = (int)Math.Round(borderThickness + (gy + 1) * periodY);

        return (x0, y0, x1 - x0, y1 - y0);
    }



    private static (int x0, int y0, int w, int h) GetPaddedTileBoundsPx(int gx, int gy, int borderThickness, double periodX, double periodY)
    {
        (int x0, int y0, int w, int h) = GetTileBoundsPx(gx, gy, borderThickness, periodX, periodY);

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



    private static FastBitmap PrepareForComparison(Bitmap tileCrop)
    {
        using Bitmap resized = ResizeTo(tileCrop, TemplateSize, TemplateSize);
        return new FastBitmap(resized);
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



    // debug output



    private static int GetNextDebugIndex()
    {
        if (!Directory.Exists("debug")) return 0;

        int max = -1;
        foreach (string file in Directory.GetFiles("debug", "*.png"))
        {
            string name = Path.GetFileNameWithoutExtension(file);
            if (int.TryParse(name, out int n) && n > max)
                max = n;
        }

        return max + 1;
    }



    // templates + classification



    private sealed class Template
    {
        public required Tile Tile;
        public required string Name;
        public required FastBitmap Image;
        public int? PortalGroup; // set when Tile == Tile.Portal; identifies which of the 3 color pairs this template represents
    }



    private static List<Template> LoadTemplates()
    {
        List<Template> list = [];

        foreach (string file in Directory.GetFiles(TemplateFolder))
        {
            string ext = Path.GetExtension(file).ToLowerInvariant();
            if (ext is not (".png" or ".bmp" or ".jpg" or ".jpeg")) continue;

            string name = Path.GetFileNameWithoutExtension(file);
            string rawPrefix = name.Split('_', '-', ' ')[0];
            string prefix = rawPrefix.TrimEnd("0123456789".ToCharArray());

            if (!Enum.TryParse(prefix, ignoreCase: true, out Tile tile))
            {
                Console.WriteLine($"Warning: couldn't parse a Tile type from '{name}', skipping.");
                continue;
            }

            int? portalGroup = null;
            if (tile == Tile.Portal)
            {
                string digits = rawPrefix[prefix.Length..];
                if (int.TryParse(digits, out int group))
                    portalGroup = group;
                else
                    Console.WriteLine($"Warning: portal template '{name}' has no group number, it won't be linkable.");
            }

            using Bitmap raw = new(file);
            using Bitmap resized = ResizeTo(raw, TemplateSize, TemplateSize);
            list.Add(new Template { Tile = tile, Name = name, Image = new FastBitmap(resized), PortalGroup = portalGroup });
        }

        return list;
    }



    private static (Tile[,] tiles, Pos start, Dictionary<int, List<Pos>> portalGroups) ClassifyTiles(
        Bitmap boardBmp, int cols, int rows, int borderThickness, double periodX, double periodY,
        List<Template> templates)
    {
        Tile[,] tiles = new Tile[cols, rows];
        Pos start = new(-1, -1);
        Dictionary<int, List<Pos>> portalGroups = new();

        int debugCounter = GetNextDebugIndex();
        bool anyUnrecognized = false;

        for (int gx = 0; gx < cols; gx++)
        {
            for (int gy = 0; gy < rows; gy++)
            {
                (int cellX0, int cellY0, int cellW, int cellH) = GetPaddedTileBoundsPx(gx, gy, borderThickness, periodX, periodY);

                using Bitmap tileCrop = ExtractTileCrop(boardBmp, cellX0, cellY0, cellW, cellH);
                using FastBitmap tileFb = PrepareForComparison(tileCrop);

                (Template? best, double score) = ClassifyTile(tileFb, templates);

                if (best is null || score > TemplateThreshold)
                {
                    string debugPath = $"debug/{debugCounter++}.png";
                    tileFb.Save(debugPath);
                    Console.WriteLine($"  Warning: no confident match at cell ({gx},{gy}) (closest {best?.Tile} @ {score:F3}) -- saved to {debugPath}");
                    anyUnrecognized = true;
                    continue;
                }

                int boardX = gx;
                int boardY = rows - 1 - gy; // screen top row -> highest board Y (matches Board.Print)
                Pos pos = new(boardX, boardY);

                tiles[boardX, boardY] = best.Tile;

                if (best.Tile == Tile.Horse)
                    start = pos;

                if (best.PortalGroup is int group)
                {
                    if (!portalGroups.TryGetValue(group, out List<Pos>? positions))
                        portalGroups[group] = positions = [];
                    positions.Add(pos);
                }
            }
        }

        if (anyUnrecognized)
            throw new InvalidOperationException(
                "One or more tiles could not be classified with confidence. Check debug/ for saved strips and add matching templates.");

        return (tiles, start, portalGroups);
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
}



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



    public void Save(string filename)
    {
        using Bitmap bmp = new(Width, Height, PixelFormat.Format24bppRgb);
        BitmapData data = bmp.LockBits(
            new Rectangle(0, 0, Width, Height),
            ImageLockMode.WriteOnly,
            PixelFormat.Format24bppRgb);

        // our internal stride may differ from the new bitmap's stride, so copy row by row
        for (int y = 0; y < Height; y++)
        {
            Marshal.Copy(_buffer, y * _stride, data.Scan0 + y * data.Stride, _stride);
        }

        bmp.UnlockBits(data);

        ImageFormat format = Path.GetExtension(filename).ToLowerInvariant() switch
        {
            ".jpg" or ".jpeg" => ImageFormat.Jpeg,
            ".bmp" => ImageFormat.Bmp,
            ".gif" => ImageFormat.Gif,
            _ => ImageFormat.Png,
        };

        string? dir = Path.GetDirectoryName(filename);
        if (!string.IsNullOrEmpty(dir))
            Directory.CreateDirectory(dir);

        bmp.Save(filename, format);
    }



    public (byte R, byte G, byte B) GetPixel(int x, int y)
    {
        int idx = y * _stride + x * 3;
        return (_buffer[idx + 2], _buffer[idx + 1], _buffer[idx]);
    }



    public void SetPixelWhite(int x, int y)
    {
        int idx = y * _stride + x * 3;
        _buffer[idx + 2] = 0xff;
        _buffer[idx + 1] = 0xff;
        _buffer[idx + 0] = 0xff;
    }



    public void Dispose() {}
}