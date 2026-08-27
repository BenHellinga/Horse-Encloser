// Program.cs
using System.Runtime.Versioning;

namespace HorseEncloser;



[SupportedOSPlatform("windows")]
public static class Program
{
    public static void Main(string[] args)
    {
        Board? board = ParseArgsAndGetBoard(args);
        if (board is null) return;

        board.Print();

        Result? result = v2.Solver.Solve(board);
        if (result == null)
        {
            Console.WriteLine("\nNo Solution");
        }
        else
        {
            Console.WriteLine($"\nArea: {result.Area}");
            Console.WriteLine(string.Join(", ", result.Walls.Select(w => $"({w.X}, {w.Y})")));
            result.Board.Print();
        }

        Console.WriteLine("\nFinished");
    }



    // argument parsing



    private static Board? ParseArgsAndGetBoard(string[] args)
    {
        int? numWalls = null;
        string? loadFilepath = null;
        string? saveFilepath = null;

        try
        {
            for (int i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "-w": numWalls = ExpectInt(args, ref i, "-w"); break;
                    case "-s": saveFilepath = ExpectValue(args, ref i, "-s"); break;
                    case "-l": loadFilepath = ExpectValue(args, ref i, "-l"); break;

                    default: throw new ArgumentException($"Unrecognized argument '{args[i]}'.");
                }
            }
        }
        catch (ArgumentException ex)
        {
            Console.WriteLine(ex.Message);
            return null;
        }

        if (numWalls is null && loadFilepath is null)
        {
            Console.WriteLine("Usage: -w <numWalls> [-s <filepath>]   OR   -f <filepath>");
            return null;
        }

        if (numWalls is not null && loadFilepath is not null)
        {
            Console.WriteLine("-w and -l cannot be used together.");
            return null;
        }

        if (loadFilepath is not null && saveFilepath is not null)
        {
            Console.WriteLine("-s is only valid alongside -w.");
            return null;
        }

        if (loadFilepath is not null)
            return Board.Load(loadFilepath);

        Board? board = WebsiteIO.ReadBoardFromScreen(numWalls!.Value);
        if (board == null) return null;
        
        if (saveFilepath is not null)
            board.Save(saveFilepath);

        return board;
    }



    private static string ExpectValue(string[] args, ref int i, string flag)
    {
        if (i + 1 >= args.Length)
            throw new ArgumentException($"'{flag}' expects a value.");

        return args[++i];
    }



    private static int ExpectInt(string[] args, ref int i, string flag)
    {
        string raw = ExpectValue(args, ref i, flag);
        if (!int.TryParse(raw, out int value))
            throw new ArgumentException($"'{flag}' expects an integer, got '{raw}'.");

        return value;
    }
}