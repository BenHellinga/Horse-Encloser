namespace HorseEncloser.v3;



public static class Solver
{
    public static Result? Solve(Board board, bool costly = false, bool verbose = false, int reports = 20)
    {
        Graph graph = Graph.FromBoard(board);
        graph.PrintNumEdges();
        List<Pos> walls = graph.GetOptimalWallPositions();

        return new(0, [], board);
    }
}