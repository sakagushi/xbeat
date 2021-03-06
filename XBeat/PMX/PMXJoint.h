#pragma once

#include "PMXDefinitions.h"
#include "PMXLoader.h"
#include "GeometricPrimitive.h"

namespace PMX {

class Model;

class Joint
{
public:
	Joint();
	~Joint();

	bool Initialize(std::shared_ptr<Physics::Environment> physics, PMX::Model *model, Loader::Joint *joint);
	void InitializeDebug(ID3D11DeviceContext *context);
	void Shutdown(std::shared_ptr<Physics::Environment> physics);

	void XM_CALLCONV Render(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection);

	std::shared_ptr<btTypedConstraint> GetConstraint() { return m_constraint; }

private:
	std::shared_ptr<btTypedConstraint> m_constraint;
	std::unique_ptr<DirectX::GeometricPrimitive> m_primitive;
	JointType m_type;
};

}

