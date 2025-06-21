#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>

class Atom;

class Graph {
public:
    void addNode(Atom* atom1, Atom* atom2) {
        if (graph.find(atom1) == graph.end()) {
            graph[node] = std::vector<int>();
        }
    }

    void addEdge(Atom* from, Atom* to) {
        graph[from].push_back(to);
    }

    // void print() {
    //     for (const auto& [node, neighbors] : adjacencyList) {
    //         std::cout << node << " -> ";
    //         for (int neighbor : neighbors) {
    //             std::cout << neighbor << " ";
    //         }
    //         std::cout << "\n";
    //     }
    // }

    const auto& getAdjacencyList() const {
        return graph;
    }

private:
    std::unordered_map<Atom*, std::vector<Atom*>> graph;
};