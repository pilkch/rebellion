#include "navigation.h"

void NavigationMesh::SetNodesAndEdges(const std::vector<spitfire::math::cVec3>& nodePositions, const std::vector<std::pair<size_t, size_t>>& _edges)
{
  // Add the nodes
  const size_t nNodePositions = nodePositions.size();
  for (size_t i = 0; i < nNodePositions; i++) {
    Node node;
    node.position = nodePositions[i];
    node.debug_identifier = i;
    nodes.push_back(node);
  }

  // Set the edges of each node
  const size_t n = _edges.size();
  for (size_t i = 0; i < n; i++) {
    Edge edge;
    edge.from = &nodes[_edges[i].first];
    edge.to = &nodes[_edges[i].second];
    edge.cost = spitfire::math::GetDistance(edge.from->position, edge.to->position);

    nodes[_edges[i].first].edges.push_back(edge);
  }
}
