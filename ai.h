#ifndef AI_H
#define AI_H

#include <list>
#include <map>
#include <memory>

#include <spitfire/spitfire.h>
#include <spitfire/math/cVec3.h>
#include <spitfire/math/cQuaternion.h>

class NavigationMesh;

typedef uint16_t aiagentid_t;

class AISystem;
struct AIAgent;

class AIGoal {
public:
  virtual bool IsSatisfied(const AISystem& ai, const AIAgent& agent) const = 0;
};

class AIGoalTakeControlPoint : public AIGoal {
public:
  explicit AIGoalTakeControlPoint(const spitfire::math::cVec3& controlPointPosition);

  virtual bool IsSatisfied(const AISystem& ai, const AIAgent& agent) const override;

  spitfire::math::cVec3 controlPointPosition;
};

class AIGoalTakeCover : public AIGoal {
};

class AIGoalReloadWeapon : public AIGoal {
};

class AIState {
public:
  virtual ~AIState() {}

  virtual void Update(const AISystem& ai, AIAgent& agent) = 0;
};

class AIStateGoto : public AIState {
public:
  explicit AIStateGoto(const spitfire::math::cVec3& _targetPosition) :
    targetPosition(_targetPosition)
  {
  }

private:
  virtual void Update(const AISystem& ai, AIAgent& agent) override;

  spitfire::math::cVec3 targetPosition;
};

class AIStateAnimate : public AIState {
public:

private:
  virtual void Update(const AISystem& ai, AIAgent& agent) override;
};


struct AIAgent {
  explicit AIAgent(const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation);

  spitfire::math::cVec3 position;
  spitfire::math::cQuaternion rotation;

  struct Blackboard {
    std::list<AIGoal*> goals;
    std::list<AIState*> states;
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
  void AddAgentGoal(aiagentid_t id, AIGoal* pGoal);

  void Update(spitfire::durationms_t currentSimulationTime);

private:
  const NavigationMesh& navigationMesh;

  std::map<aiagentid_t, std::unique_ptr<AIAgent> > agents;
};

#endif // AI_H
