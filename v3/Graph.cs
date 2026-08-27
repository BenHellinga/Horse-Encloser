using System.Threading.Tasks.Dataflow;

namespace HorseEncloser.v3;



public class Graph
{
    public int Width;
    public int Height;
    public readonly Node?[,] Nodes;
    public int NodeCount;
    public Node Horse;
    public int NumWalls;



    public class Node(int id, int x, int y, TileAttributes tile, Node[]? edges = null)
    {
        public int Id = id;
        public int X = x;
        public int Y = y;
        public TileAttributes Tile = tile;
        public Node[] Edges = edges ?? [];
    }



    private class BlockGroup(Node[] starts, int minWalls, int minPathLength, Node minPathNode, Node[]? minWallBackstop, Blocking[] blockings)
    {
        public int MinWalls = minWalls;
        public int MinPathLength = minPathLength;
        public Node MinPathNode = minPathNode;
        public Node[] Starts = starts;
        public Node[]? MinBackstop = minWallBackstop;
        public Blocking[] Blockings = blockings;
    }



    private class Blocking(int numWalls, int points, Node[] walls)
    {
        public int NumWalls = numWalls;
        public int Points = points;
        public Node[] Walls = walls;
    }



    private struct BfsState
    {
        public int[] Visited;
        public int[] End;
        public Node?[] Parent;
        public Node?[] Queue;
        public int QueueHead;
        public int QueueCount;
        public int Stamp;
    }
    private BfsState _bfs;



    private struct BspState
    {
        public int DefaultPoints;
        public Node[] BlockNodes;
        public Node[] CurrentWalls;
    }
    private BspState _bsp;



    // constructor



    public Graph(int width, int height)
    {
        Width = width;
        Height = height;
        Nodes = new Node?[width, height];
        NodeCount = 0;
        Horse = null!;
        NumWalls = -1;

        int maxNodes = width * height;
        _bfs = new BfsState
        {
            Visited = new int[maxNodes],
            End = new int[maxNodes],
            Parent = new Node?[maxNodes],
            Queue = new Node?[maxNodes],
            QueueHead = 0,
            QueueCount = 0,
            Stamp = 0,
        };

        _bsp = new BspState
        {
            DefaultPoints = -1,
            BlockNodes = null!,
            CurrentWalls = null!,
        };
    }



    // printing



    public void PrintNumEdges()
    {
        for (int y = Height - 1; y >= 0; --y)
        {
            for (int x = 0; x < Width; ++x)
            {
                Node? node = Nodes[x, y];

                if (node == null)
                    Console.Write("  ");
                else
                    Console.Write($"{node.Edges.Length} ");
            }

            Console.WriteLine();
        }
    }



    // converting board -> graph



    public static Graph FromBoard(Board board)
    {
        Graph graph = new(board.Width, board.Height);
        graph.NumWalls = board.NumWalls;
        int maxNodes = board.Width * board.Height;
        Node?[] byId = new Node?[maxNodes];
        List<Node>[] edgeLists = new List<Node>[maxNodes];
        int nextId = 0;

        // loop over every tile on board and convert to graph node
        for (int x = 0; x < board.Width; ++x)
        for (int y = 0; y < board.Height; ++y)
        {
            // only include walkable tiles
            Tile tile = board.Tiles[x, y];
            if (!Board.Attributes[(int)tile].Walkable) continue;

            // build adjacent list from Board.Directions and board.PortalDestinations
            List<(int, int)> adjacent = [];

            foreach ((int dx, int dy) in Board.Directions)
                adjacent.Add((x + dx, y + dy));

            if (tile == Tile.Portal)
            {
                Pos dest = board.PortalDestinations[new(x, y)];
                adjacent.Add((dest.X, dest.Y));
            }

            // make new node
            Node node = new(nextId, x, y, Board.Attributes[(int)tile]);
            graph.Nodes[x, y] = node;
            byId[nextId] = node;
            edgeLists[nextId] = [];
            nextId++;

            // loop through adjacent
            bool isEdge = board.IsEdge(new(x, y));
            foreach ((int nx, int ny) in adjacent)
            {
                Pos pos = new(nx, ny);

                // if adjacent tile is off board or another border tile
                if (board.IsOutside(pos)) continue;
                if (isEdge && board.IsEdge(pos)) continue;

                // if adjacent tile hasnt been added yet or will not be added
                Node? next = graph.Nodes[nx, ny];
                if (next == null) continue;

                // add edges to node and node to edges
                edgeLists[node.Id].Add(next);
                edgeLists[next.Id].Add(node);
            }
        }

        // bake adjacency lists into arrays now that they're final
        for (int i = 0; i < nextId; i++)
            byId[i]!.Edges = [.. edgeLists[i]];

        graph.NodeCount = nextId;
        graph.Horse = graph.Nodes[board.Start.X, board.Start.Y]!;
        graph.RemoveUnReachableNodes();
        return graph;
    }



    private void RemoveUnReachableNodes()
    {
        BFS([Horse], []);
        int stamp = _bfs.Stamp;

        // check for nodes that were not visited
        foreach (Node? node in Nodes)
        {
            if (node == null) continue;
            if (_bfs.Visited[node.Id] == stamp) continue;

            // drop from graph
            Nodes[node.X, node.Y] = null;
        }

        ReassignIds();
    }



    private void ReassignIds()
    {
        int nextId = 0;

        // assign new ids
        foreach (Node? node in Nodes)
        {
            if (node == null) continue;
            node.Id = nextId++;
        }

        NodeCount = nextId;
        _bfs = new BfsState
        {
            Visited = new int[NodeCount],
            End = new int[NodeCount],
            Parent = new Node?[NodeCount],
            Queue = new Node?[NodeCount],
            QueueHead = 0,
            QueueCount = 0,
            Stamp = 0,
        };
    }



    // bfs



    public (Node[]? path, int? points) BFS(List<Node> starts, List<Node> ends)
    {
        int stamp = ++_bfs.Stamp;
        _bfs.QueueHead = 0;
        _bfs.QueueCount = 0;
        int points = 0;

        for (int i = 0; i < starts.Count; i++)
        {
            Node start = starts[i];
            if (!start.Tile.Walkable) continue;

            _bfs.Visited[start.Id] = stamp;
            _bfs.Parent[start.Id] = null;
            Enqueue(start);
            points += start.Tile.Value;
        }

        for (int i = 0; i < ends.Count; i++)
            _bfs.End[ends[i].Id] = stamp;

        while (TryDequeue(out Node? node))
        {
            foreach (Node next in node!.Edges)
            {
                if (_bfs.Visited[next.Id] == stamp) continue;
                if (!next.Tile.Walkable) continue;

                _bfs.Visited[next.Id] = stamp;
                _bfs.Parent[next.Id] = node;

                if (_bfs.End[next.Id] == stamp)
                    return (RecoverPath(next), null);

                Enqueue(next);
                points += next.Tile.Value;
            }
        }

        return (null, points);
    }



    private void Enqueue(Node node)
    {
        _bfs.Queue[(_bfs.QueueHead + _bfs.QueueCount) % _bfs.Queue.Length] = node;
        _bfs.QueueCount++;
    }



    private bool TryDequeue(out Node? node)
    {
        if (_bfs.QueueCount == 0)
        {
            node = null;
            return false;
        }

        node = _bfs.Queue[_bfs.QueueHead];
        _bfs.QueueHead = (_bfs.QueueHead + 1) % _bfs.Queue.Length;
        _bfs.QueueCount--;
        return true;
    }



    private Node[] RecoverPath(Node start)
    {
        int length = 1;
        Node current = start;

        while (_bfs.Parent[current.Id] != null)
        {
            current = _bfs.Parent[current.Id]!;
            length++;
        }

        Node[] path = new Node[length];
        current = start;

        for (int i = length - 1; i >= 0; i--)
        {
            path[i] = current;
            Node? parent = _bfs.Parent[current.Id];
            if (parent != null) current = parent;
        }

        return path;
    }



    // sensible tiles



    public List<Pos> GetOptimalWallPositions()
    {
        (Node[]? _, int? points) = BFS([Horse], []);
        _bsp.DefaultPoints = points!.Value;
        _bsp.BlockNodes = GetBlockNodes();
        _bsp.CurrentWalls = new Node[NumWalls];

        BlockGroup[] blockGroups = GetBaseBlockGroups();


        return [];
    }



    public Node[] GetBlockNodes()
    {
        List<Node> nodes = [];

        foreach (Node? node in Nodes)
        {
            if (node is null) continue;

            if (node.Tile.Value < 1) nodes.Add(node);
            else if (node.X == 0 || node.X == Width - 1 || node.Y == 0 || node.Y == Height - 1) nodes.Add(node);
        }

        return [.. nodes];
    }



    private BlockGroup[] GetBaseBlockGroups()
    {
        List<BlockGroup> groups = [];

        Node[] blockNodes = _bsp.BlockNodes;
        for (int i = 0; i < blockNodes.Length; ++i)
        {
            Node[] starts = [blockNodes[i]];

            Node[] ends = new Node[blockNodes.Length];
            Array.Copy(blockNodes, ends, i);
            Array.Copy(blockNodes, i + 1, ends, i, blockNodes.Length - i - 1);
            ends[blockNodes.Length - 1] = Horse;

            BlockGroup group = ComputeBlockGroup(starts, ends, null, -1);

            Console.WriteLine($"{group.MinWalls} {group.MinPathLength}, ({group.MinPathNode.X}, {group.MinPathNode.Y}), {group.MinBackstop!.Length}");
            foreach (Blocking blocking in group.Blockings)
            {
                if (blocking is null) continue;
                Console.WriteLine($"{blocking.NumWalls}, {string.Join(", ", blocking.Walls.Select(w => $"({w.X}, {w.Y})"))}");
            }
            Console.WriteLine();
        }


        return [.. groups];
    }



    private BlockGroup ComputeBlockGroup(Node[] starts, Node[] ends, Node[]? backstop, int maxWalls)
    {
        Blocking[] blockings = new Blocking[NumWalls + 1];

        int minPathLength;
        Node minPathNode;

        bool minWallsSet = false;
        int minWalls = maxWalls;
        Node[]? minBackstop = null;

        Node[]? lastFrontStop = null;
        Node[]? currentFrontStop = null;

        if (maxWalls == -1)
        {
            (Blocking? maxBlocking, Node[]? maxBackstop, minPathLength, minPathNode) = ComputeMaxBlocking(starts, ends);

            if (maxBlocking == null)
                return new(starts, -1, minPathLength, minPathNode, null, blockings);
        
            int walls = maxBlocking.Walls.Length;
            blockings[walls] = maxBlocking;
            maxWalls = walls;

            minBackstop = maxBackstop;
            backstop = maxBackstop;
            minWalls = maxWalls;
        }
        else
        {
            (Node[]? path, int? _) = BFS(starts, ends, backstop);
            minPathLength = path!.Length;
            minPathNode = path.Last();
        }

        int numWalls = 0;

        void RecursiveBlockShortestPath(int wallsUsed)
        {
            (Node[]? path, int? _) = BFS(starts, ends, backstop); // needs to be updated to be the complex version

            if (path is null)
            {
                if (!minWallsSet)
                {
                    minWallsSet = true;
                    minWalls = wallsUsed;
                }

                Node[] currentWalls = new Node[wallsUsed];
                Array.Copy(_bsp.CurrentWalls, currentWalls, wallsUsed);

                List<Node> backstop = [];
                foreach (Node wall in currentWalls)
                    foreach (Node edge in wall.Edges)
                        if (_bfs.Visited[edge.Id]-- == _bfs.Stamp && edge.Tile.Type != Tile.Wall)
                            backstop.Add(edge);

                (Node[]? _, int? points) = BFS([Horse], []);

                if (blockings[wallsUsed] is null)
                {
                    blockings[wallsUsed] = new(wallsUsed, points!.Value, currentWalls);
                }
                else if (points!.Value <= blockings[wallsUsed].Points)
                {
                    blockings[wallsUsed].Points = points!.Value;
                    blockings[wallsUsed].Walls = currentWalls;
                }
                else return;

                if (wallsUsed == minWalls)
                    minBackstop = [.. backstop];

                return;
            }

            if (wallsUsed == numWalls) return;

            foreach (Node node in path)
            {
                if (!node.Tile.Placeable) continue;

                TileAttributes tile = node.Tile;
                node.Tile = Board.Attributes[(int)Tile.Wall];
                _bsp.CurrentWalls[wallsUsed] = node;

                RecursiveBlockShortestPath(wallsUsed + 1);

                node.Tile = tile;
            }
        }

        while (++numWalls < maxWalls) RecursiveBlockShortestPath(0);
        return new(blockings[minWalls].Walls, minWalls, minPathLength, minPathNode, minBackstop, blockings);
    }



    private (Blocking? blocking, Node[]? maxBackstop, int minPathLength, Node minPathNode) ComputeMaxBlocking(Node[] starts, Node[] ends)
    {
        Blocking? blocking = null;
        Node[]? maxBackstop = null;
        Node[]? minpath = null;

        void RecursiveBlockShortestPath(int wallsUsed)
        {
            (Node[]? path, int? _) = BFS(starts, ends, null);

            if (path == null)
            {
                Node[] currentWalls = new Node[wallsUsed];
                Array.Copy(_bsp.CurrentWalls, currentWalls, wallsUsed);

                List<Node> backstop = [];
                foreach (Node wall in currentWalls)
                    foreach (Node edge in wall.Edges)
                        if (_bfs.Visited[edge.Id]-- == _bfs.Stamp)
                            backstop.Add(edge);
                maxBackstop = [.. backstop];

                (Node[]? _, int? points) = BFS([Horse], []);

                blocking = new(wallsUsed, points!.Value, currentWalls);
                return;
            }

            if (wallsUsed == 0)
                minpath = path;

            foreach (Node node in path)
            {
                if (!node.Tile.Placeable) continue;

                TileAttributes tile = node.Tile;
                node.Tile = Board.Attributes[(int)Tile.Wall];
                _bsp.CurrentWalls[wallsUsed] = node;

                RecursiveBlockShortestPath(wallsUsed + 1);

                node.Tile = tile;
                return;
            }
        }

        RecursiveBlockShortestPath(0);
        return (blocking, maxBackstop, minpath!.Length, minpath.Last());
    }



    public (Node[]? path, int? points) BFS(Node[] starts, Node[] ends, Node[]? backstop)
    {
        int stamp = ++_bfs.Stamp;
        _bfs.QueueHead = 0;
        _bfs.QueueCount = 0;
        int points = 0;

        for (int i = 0; i < starts.Length; i++)
        {
            Node start = starts[i];
            if (!start.Tile.Walkable) continue;

            _bfs.Visited[start.Id] = stamp;
            _bfs.Parent[start.Id] = null;
            Enqueue(start);
            points += start.Tile.Value;
        }

        for (int i = 0; i < ends.Length; i++)
            _bfs.End[ends[i].Id] = stamp;

        while (TryDequeue(out Node? node))
        {
            foreach (Node next in node!.Edges)
            {
                if (_bfs.Visited[next.Id] == stamp) continue;
                if (!next.Tile.Walkable) continue;

                _bfs.Visited[next.Id] = stamp;
                _bfs.Parent[next.Id] = node;

                if (_bfs.End[next.Id] == stamp)
                    return (RecoverPath(next), null);

                Enqueue(next);
                points += next.Tile.Value;
            }
        }

        return (null, points);
    }
}