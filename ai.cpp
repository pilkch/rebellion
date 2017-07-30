#include <cassert>

#include <algorithm>

#include "ai.h"
#include "astar.h"
#include "navigation.h"

AIGoalTakeControlPoint::AIGoalTakeControlPoint(const spitfire::math::cVec3& _controlPointPosition) :
  controlPointPosition(_controlPointPosition)
{
}

bool AIGoalTakeControlPoint::IsSatisfied(const AISystem& ai, const AIAgent& agent) const
{
  const float fRadius = 1.0f;
  return (agent.position - controlPointPosition).GetLength() < fRadius;
}


AIGoalPatrol::AIGoalPatrol(const std::list<spitfire::math::cVec3>& _patrolPoints) :
  patrolPoints(_patrolPoints)
{
}

bool AIGoalPatrol::IsSatisfied(const AISystem& ai, const AIAgent& agent) const
{
  return patrolPoints.empty();
}

void AIGoalPatrol::Update(const AISystem& ai, const AIAgent& agent)
{
  if (!patrolPoints.empty()) {
    // Check if we are close enough to the next patrol point to be able to count it as visited
    auto&& point = patrolPoints.front();
    const float fRadius = 1.0f;
    if ((agent.position - point).GetLength() < fRadius) {
      // We were close enough to the first patrol point so we can remove it
      patrolPoints.pop_front();
    }
  }
}


void AIActionGoto::Update(const AISystem& ai, AIAgent& agent)
{
  spitfire::math::cVec3 target = targetPosition;

  // If we have a path to move along get the first node and move to it first
  if (!path.empty()) {
    target = path.front().position;
  }


  // Update agent
  const float fSpeed = 0.1f;
  spitfire::math::cVec3& position(agent.position);
  const float fDistance = spitfire::math::cVec3(target - position).GetLength();
  if (fDistance < 2.0f) {
    // Close enough to just move the agent to the target position
    position = target;

    // We are now at the first path node, so pop it to move onto the next one
    if (!path.empty()) {
      path.pop_front();
    }
  } else if (fDistance < 6.0f) {
    // Ease into the target position
    float fEasing = 0.1f;
    const spitfire::math::cVec3 direction = (target - position).GetNormalised();
    position += std::min(fSpeed, (fEasing * fDistance)) * direction;
  } else {
    // Move at a constant speed to the target position
    const spitfire::math::cVec3 direction = (target - position).GetNormalised();
    position += fSpeed * direction;
  }
}

AIAgent::AIAgent(const spitfire::math::cVec3& _position, const spitfire::math::cQuaternion& _rotation) :
  position(_position),
  rotation(_rotation)
{
}


AISystem::AISystem(const NavigationMesh& _navigationMesh) :
  navigationMesh(_navigationMesh)
{
}

aiagentid_t AISystem::AddAgent(const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation)
{
  const aiagentid_t n = std::numeric_limits<aiagentid_t>::max();
  for (aiagentid_t id = 0; id < n; id++) {
    if (agents.find(id) == agents.end()) {
      agents.insert(std::make_pair(id, std::unique_ptr<AIAgent>(new AIAgent(position, rotation))));
      return id;
    }
  }

  assert(false);
  return 0;
}

void AISystem::RemoveAgent(aiagentid_t id)
{
  agents.erase(id);
}

const spitfire::math::cVec3& AISystem::GetAgentPosition(aiagentid_t id) const
{
  auto iter = agents.find(id);
  assert(iter != agents.end());

  return iter->second->position;
}

void AISystem::SetAgentPositionAndRotation(aiagentid_t id, const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation)
{
  auto iter = agents.find(id);
  if (iter != agents.end()) {
    iter->second->position = position;
    iter->second->rotation = rotation;
  }
}

size_t AISystem::GetAgentGoalCount(aiagentid_t id) const
{
  auto iter = agents.find(id);
  if (iter != agents.end()) return iter->second->blackboard.goals.size();

  return 0;
}

size_t AISystem::GetAgentActionCount(aiagentid_t id) const
{
  auto iter = agents.find(id);
  if (iter != agents.end()) return iter->second->blackboard.actions.size();

  return 0;
}

bool AISystem::GetAgentGoalPosition(aiagentid_t id, spitfire::math::cVec3& goalPosition) const
{
  auto iter = agents.find(id);
  if (iter != agents.end()) {
    if (!iter->second->blackboard.goals.empty()) {
      AIGoal* pGoal = *(iter->second->blackboard.goals.begin());
      AIGoalTakeControlPoint* pGoalTakeControlPoint = (AIGoalTakeControlPoint*)pGoal;
      goalPosition = pGoalTakeControlPoint->controlPointPosition;
      return true;
    }
  }

  return false;
}

void AISystem::AddAgentGoal(aiagentid_t id, AIGoal* pGoal)
{
  auto iter = agents.find(id);
  if (iter != agents.end()) iter->second->blackboard.goals.push_back(pGoal);
}

void AISystem::Update(spitfire::durationms_t currentSimulationTime)
{
  // Update agents
  for (auto& iter : agents) {
    AIAgent& agent = *(iter.second);

    // No goals, nothing to do
    if (agent.blackboard.goals.empty()) continue;

    // Remove any goals that are satisfied
    const size_t nGoalsBefore = agent.blackboard.goals.size();

    std::list<AIGoal*> satisfiedGoals;

    // Work out which goals are satisfied and can be removed
    for (auto& pGoal : agent.blackboard.goals) {
      if (pGoal->IsSatisfied(*this, agent)) {
        satisfiedGoals.push_back(pGoal);
      }
    }

    // Remove and delete the satisfied goals
    for (auto& pGoal : satisfiedGoals) {
      delete pGoal;

      agent.blackboard.goals.remove(pGoal);
    }

    // HACK: If we removed any goals then remove all our actions
    if ((nGoalsBefore - agent.blackboard.goals.size()) != 0) {
      // Delete all actions
      for (auto& pAction : agent.blackboard.actions) {
        delete pAction;
      }

      // Remove all actions
      agent.blackboard.actions.clear();
    }

    // No goals any more, nothing to do
    if (agent.blackboard.goals.empty()) continue;

    // If we don't have an action yet then we need to work out which action can satisfy our primary goal and add it
    if (agent.blackboard.actions.empty()) {
      // TODO: Work out which actions satisfy our goals and add them

      const Node* pNodeFrom = navigationMesh.GetClosestNodeToPoint(agent.position);
      ASSERT(pNodeFrom != nullptr);

      AIGoal* pGoal = *(agent.blackboard.goals.begin());
      AIGoalTakeControlPoint* pGoalTakeControlPoint = (AIGoalTakeControlPoint*)pGoal;

      const Node* pNodeTo = navigationMesh.GetClosestNodeToPoint(pGoalTakeControlPoint->controlPointPosition);
      ASSERT(pNodeTo != nullptr);

      std::list<Node> path;

      // If our starting node is not the same node as our end node then we need to find out the path between them
      if ((pNodeFrom != nullptr) && (pNodeTo != nullptr) && (pNodeFrom != pNodeTo)) {
        astar::config<Node> cfg;
        cfg.node_limit = 1000;
        cfg.cost_limit = 1000;
        cfg.route_cost = 0.0f;

        astar::astar(*pNodeFrom, *pNodeTo, path, &astar::straight_distance_heuristic<Node>, cfg);
      }

      agent.blackboard.actions.push_back(new AIActionGoto(path, pGoalTakeControlPoint->controlPointPosition));
    }

    for (auto pAction : agent.blackboard.actions) {
      pAction->Update(*this, agent);
    }
  }
}
