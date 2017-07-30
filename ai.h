#ifndef AI_H
#define AI_H

#include <list>
#include <map>
#include <memory>

#include <spitfire/spitfire.h>
#include <spitfire/math/cVec3.h>
#include <spitfire/math/cQuaternion.h>

struct Node;
class NavigationMesh;

typedef uint16_t aiagentid_t;

class AISystem;
struct AIAgent;

class AIGoal {
public:
  virtual bool IsSatisfied(const AISystem& ai, const AIAgent& agent) const = 0;
  virtual void Update(const AISystem& ai, const AIAgent& agent) {}
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

class AIGoalPatrol : public AIGoal {
public:
  explicit AIGoalPatrol(const std::list<spitfire::math::cVec3>& patrolPoints);

  virtual bool IsSatisfied(const AISystem& ai, const AIAgent& agent) const override;
  virtual void Update(const AISystem& ai, const AIAgent& agent) override;

  std::list<spitfire::math::cVec3> patrolPoints;
};

class AIAction {
public:
  virtual ~AIAction() {}

  virtual void Update(const AISystem& ai, AIAgent& agent) = 0;
};

class AIActionGoto : public AIAction {
public:
  explicit AIActionGoto(const std::list<Node>& _path, const spitfire::math::cVec3& _targetPosition) :
    path(_path),
    targetPosition(_targetPosition)
  {
  }

private:
  virtual void Update(const AISystem& ai, AIAgent& agent) override;

  std::list<Node> path;
  spitfire::math::cVec3 targetPosition;
};

class AIActionAnimate : public AIAction {
public:

private:
  virtual void Update(const AISystem& ai, AIAgent& agent) override;
};


class AIActionLookAtPoint : public AIAction {
public:
  explicit AIActionLookAtPoint(const spitfire::math::cVec3& _targetPosition) :
    targetPosition(_targetPosition)
  {
  }

private:
  virtual void Update(const AISystem& ai, AIAgent& agent) override;

  /*
  // Look at a our target point
  LTVector vDir = targetPosition - ai.GetPosition();
  vDir.Normalize();

  if ( vDir.Dot( ai.GetForwardVector() ) < 0.f ) {
    // The target is in front of us, so look at it
    animProps.Set( kAPG_Action, kAP_ACT_Alert );
    pAI->GetAIBlackBoard()->SetBBFacePos( vTargetPos );
  } else if ( vDir.Dot( ai.GetForwardVector() ) > c_fFOV60 ) {
  animProps.Set( kAPG_Action, kAP_ACT_Alert );
  } else if ( vDir.Dot( ai.GetRightVector() ) > 0.0f ) {
    // The target is to the right so turn to the right
    animProps.Set( kAPG_Action, kAP_ACT_LookRight );
  } else {
    // The target is to the left so turn to the left
    animProps.Set( kAPG_Action, kAP_ACT_LookLeft );
  }
  */

  spitfire::math::cVec3 targetPosition;
};



struct AIAgent {
  explicit AIAgent(const spitfire::math::cVec3& position, const spitfire::math::cQuaternion& rotation);

  spitfire::math::cVec3 position;
  spitfire::math::cQuaternion rotation;

  struct Blackboard {
    std::list<AIGoal*> goals;
    std::list<AIAction*> actions;
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

  size_t GetAgentGoalCount(aiagentid_t id) const;
  size_t GetAgentActionCount(aiagentid_t id) const;

  bool GetAgentGoalPosition(aiagentid_t id, spitfire::math::cVec3& goalPosition) const;
  void AddAgentGoal(aiagentid_t id, AIGoal* pGoal);

  void Update(spitfire::durationms_t currentSimulationTime);

private:
  const NavigationMesh& navigationMesh;

  std::map<aiagentid_t, std::unique_ptr<AIAgent> > agents;
};

#endif // AI_H
