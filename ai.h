#ifndef AI_H
#define AI_H

#include <map>
#include <memory>

#include <spitfire/spitfire.h>
#include <spitfire/math/cVec3.h>
#include <spitfire/math/cQuaternion.h>

class NavigationMesh;

typedef uint16_t aiagentid_t;

struct AIAgent {
  explicit AIAgent(const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation);

  spitfire::math::cVec3 position;
  spitfire::math::cQuaternion rotation;

  struct Blackboard {
    std::unique_ptr<spitfire::math::cVec3> goalPosition;
  };
  Blackboard blackboard;
};

class AISystem {
public:
  explicit AISystem(const NavigationMesh& navigationMesh);

  aiagentid_t AddAgent(const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation);
  void RemoveAgent(aiagentid_t id);

  const spitfire::math::cVec3& GetAgentPosition(aiagentid_t id) const;
  void SetAgentPositionAndRotation(aiagentid_t id, const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation);

  bool GetAgentGoalPosition(aiagentid_t id, spitfire::math::cVec3& goalPosition) const;
  void SetAgentGoalPosition(aiagentid_t id, const spitfire::math::cVec3& goalPosition);

  void Update(spitfire::durationms_t currentSimulationTime);

private:
  const NavigationMesh& navigationMesh;

  std::map<aiagentid_t, std::unique_ptr<AIAgent> > agents;
};

#endif // AI_H
