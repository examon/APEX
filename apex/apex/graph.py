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
        path = queue.pop() #list
        node = path[-1] #str
        if node == end:
            return path
        for adjacent in graph[node]:
            # adjacent: list
            new_path = list(path)
            new_path.append(adjacent)
            queue.append(new_path)

print bfs(graph, 'main', 'y')