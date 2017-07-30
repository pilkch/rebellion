#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <spitfire/math/math.h>
#include <spitfire/math/cVec3.h>

struct Node;

struct Edge {
  Node* from;
  Node* to;
  float cost;

  bool operator==(const Edge& rhs) const { return ((from == rhs.from) && (to == rhs.to) && spitfire::math::IsApproximatelyEqual(cost, rhs.cost)); }
};

typedef float cost_type; //typedef required, must be scalar type

struct Node {
  // Iterator for fetching connected nodes, and costs
  struct iterator {
    iterator(std::vector<Edge>::iterator _iter) : iter(_iter) {}

    //copy constructor and assignment operator should be available

    typedef float cost_type; //typedef required, must be scalar type

                             // Node
    const Node& value() const { return *(iter->to); }

    // Cost/distance to node
    cost_type cost() const { return iter->cost; }

    // Next node
    iterator& operator++() { iter++; return *this; }

    // Used by search
    bool operator!=(const iterator& rhs) { return (iter != rhs.iter); }

  private:
    std::vector<Edge>::iterator iter;
  };

  // Get first, and past-end iterators
  iterator begin() { return iterator(edges.begin()); }
  iterator end() { return iterator(edges.end()); }

  // Equality operator, required
  // note: fuzzy equality may be useful
  bool operator==(const Node& rhs) const { return (position.IsApproximatelyEqual(rhs.position) && (edges == rhs.edges)); }

  spitfire::math::cVec3 position;

  std::vector<Edge> edges;

  size_t debug_identifier;
};


class NavigationMesh {
public:
  void SetNodesAndEdges(const std::vector<spitfire::math::cVec3>& nodePositions, const std::vector<std::pair<size_t, size_t>>& _edges);

  size_t GetNodeCount() const { return nodes.size(); }
  size_t GetEdgeCount() const { return edges.size(); }

  const Node& GetNode(size_t index) const { return nodes[index]; }
  Node& GetNode(size_t index) { return nodes[index]; }
  const Edge& GetEdge(size_t index) const { return edges[index]; }
  Edge& GetEdge(size_t index) { return edges[index]; }

  const Node* GetClosestNodeToPoint(const spitfire::math::cVec3& position) const;

private:
  std::vector<Node> nodes;
  std::vector<Edge> edges;
};

#endif // NAVIGATION_H
