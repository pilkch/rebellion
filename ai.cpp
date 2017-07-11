#include <cassert>

#include <algorithm>

#include "ai.h"

AIGoalTakeControlPoint::AIGoalTakeControlPoint(const spitfire::math::cVec3& _controlPointPosition) :
  controlPointPosition(_controlPointPosition)
{
}

bool AIGoalTakeControlPoint::IsSatisfied(const AISystem& ai, const AIAgent& agent) const
{
  const float fRadius = 1.0f;
  return (agent.position - controlPointPosition).GetLength() < fRadius;
}


void AIStateGoto::Update(const AISystem& ai, AIAgent& agent)
{
  // Update agent
  const float fSpeed = 0.1f;
  spitfire::math::cVec3& position(agent.position);
  const float fDistance = spitfire::math::cVec3(targetPosition - position).GetLength();
  if (fDistance < 0.1f) {
    // Close enough to just move the agent to the target position
    position = targetPosition;
  } else if (fDistance < 6.0f) {
    // Ease into the target position
    float fEasing = 0.1f;
    const spitfire::math::cVec3 direction = (targetPosition - position).GetNormalised();
    position += std::min(fSpeed, (fEasing * fDistance)) * direction;
  } else {
    // Move at a constant speed to the target position
    const spitfire::math::cVec3 direction = (targetPosition - position).GetNormalised();
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

    // HACK: If we removed any goals then remove all our states
    if ((nGoalsBefore - agent.blackboard.goals.size()) != 0) {
      // Delete all states
      for (auto& pState : agent.blackboard.states) {
        delete pState;
      }

      // Remove all states
      agent.blackboard.states.clear();
    }

    // No goals any more, nothing to do
    if (agent.blackboard.goals.empty()) continue;

    // If we don't have a state yet then we need to work out which state can satisfy our primary goal and add it
    if (agent.blackboard.states.empty()) {
      // TODO: Work out which states satisfy our goals and add them

      AIGoal* pGoal = *(agent.blackboard.goals.begin());
      AIGoalTakeControlPoint* pGoalTakeControlPoint = (AIGoalTakeControlPoint*)pGoal;
      agent.blackboard.states.push_back(new AIStateGoto(pGoalTakeControlPoint->controlPointPosition));
    }

    for (auto pState : agent.blackboard.states) {
      pState->Update(*this, agent);
    }
  }
}
