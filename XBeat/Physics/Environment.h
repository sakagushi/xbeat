#pragma once

#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodySolvers.h>
#include <memory>
#include <set>

namespace Renderer {
	class D3DRenderer;
}

namespace Physics {
enum struct PauseState {
	Running,
	Paused,
	Holding
};

class Environment
{
public:
	Environment(void);
	~Environment(void);

	bool Initialize(std::shared_ptr<Renderer::D3DRenderer> d3d);
	void Shutdown();
	bool Frame(float frameTimeMsec);

	__forceinline void Pause() { m_pauseState = PauseState::Paused; }
	__forceinline void Hold() { m_pauseState = PauseState::Holding; }
	__forceinline void Unpause() { m_pauseState = PauseState::Running; }
	__forceinline PauseState GetPauseState() { return m_pauseState; }
	__forceinline bool IsRunning() { return m_pauseState == PauseState::Running; }
	__forceinline bool IsPaused() { return m_pauseState == PauseState::Paused; }
	__forceinline bool IsHolding() { return m_pauseState == PauseState::Holding; }
	__forceinline float GetPauseTime() { return m_pauseTime; }
	__forceinline btSoftBodyWorldInfo& GetWorldInfo() { return m_dynamicsWorld->getWorldInfo(); }

	void AddSoftBody(std::shared_ptr<btSoftBody> body, int16_t group, int16_t mask);
	void RemoveSoftBody(std::shared_ptr<btSoftBody> body);

	void AddRigidBody(std::shared_ptr<btRigidBody> body, int16_t group = -1, int16_t mask = -1);
	void RemoveRigidBody(std::shared_ptr<btRigidBody> body);

	void AddConstraint(std::shared_ptr<btTypedConstraint> constraint);
	void RemoveConstraint(std::shared_ptr<btTypedConstraint> constraint);

	void AddCharacter(std::shared_ptr<btActionInterface> character);
	void RemoveCharacter(std::shared_ptr<btActionInterface> character);

private:
	PauseState m_pauseState;
	float m_pauseTime;

	std::set<std::shared_ptr<btSoftBody>> m_softBodies;
	std::set<std::shared_ptr<btRigidBody>> m_rigidBodies;
	std::set<std::shared_ptr<btActionInterface>> m_characters;
	std::set<std::shared_ptr<btTypedConstraint>> m_constraints;
	std::unique_ptr<btBroadphaseInterface> m_broadphase;
	std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
	std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
	std::unique_ptr<btConstraintSolver> m_constraintSolver;
	std::unique_ptr<btSoftRigidDynamicsWorld> m_dynamicsWorld;
	std::unique_ptr<btSoftBodySolver> m_softBodySolver;
};

}
