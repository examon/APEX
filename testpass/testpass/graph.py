# graph is in adjacent list representation
graph = {
    'main': ['x', 'a'],
    'x': ['y'],
    'a': ['b'],
    'y': [],
    'b': [],
}
print graph
print

def bfs(graph, start, end):
    queue = []
    queue.append([start])
    while queue:
        print queue
        path = queue.pop(0)
        print path
        print "-"
        node = path[-1]
        if node == end:
            return path
        for adjacent in graph[node]:
            new_path = list(path)
            new_path.append(adjacent)
            queue.append(new_path)

print bfs(graph, 'main', 'y')