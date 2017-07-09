#include <cassert>

#include <algorithm>

#include "ai.h"

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
    if (iter->second->blackboard.goalPosition != nullptr) {
      goalPosition = *(iter->second->blackboard.goalPosition);
      return true;
    }
  }

  return false;
}

void AISystem::SetAgentGoalPosition(aiagentid_t id, const spitfire::math::cVec3& goalPosition)
{
  auto iter = agents.find(id);
  if (iter != agents.end()) iter->second->blackboard.goalPosition.reset(new spitfire::math::cVec3(goalPosition));
}

void AISystem::Update(spitfire::durationms_t currentSimulationTime)
{
  // Update objects
  const float fSpeed = 0.1f;
  for (auto& iter : agents) {
    if (iter.second->blackboard.goalPosition != nullptr) {
      spitfire::math::cVec3& position(iter.second->position);
      spitfire::math::cVec3& goalPosition(*(iter.second->blackboard.goalPosition));
      const float fDistance = spitfire::math::cVec3(goalPosition - position).GetLength();
      if (fDistance < 0.1f) {
        // Close enough to just move the object to the target position
        position = goalPosition;
      } else if (fDistance < 6.0f) {
        // Ease into the target position
        float fEasing = 0.1f;
        const spitfire::math::cVec3 direction = (goalPosition - position).GetNormalised();
        position += std::min(fSpeed, (fEasing * fDistance)) * direction;
      } else {
        // Move at a constant speed to the target position
        const spitfire::math::cVec3 direction = (goalPosition - position).GetNormalised();
        position += fSpeed * direction;
      }
    }
  }
}
