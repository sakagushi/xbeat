﻿#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <array>

#include <DirectXMath.h>

#include "../Renderer/Model.h"
#include "../Physics/Environment.h"

#include "PMXDefinitions.h"
#include "PMXLoader.h"
#include "PMXSoftBody.h"
#include "PMXRigidBody.h"
#include "PMXJoint.h"
#include "PMXShader.h"
#include "PMXBone.h"

namespace PMX {

class Model : public Renderer::Model
{
public:
	Model(void);
	virtual ~Model(void);

	ModelDescription description;

	DirectX::XMVECTOR GetBonePosition(const std::wstring &JPname);
	DirectX::XMVECTOR GetBoneEndPosition(const std::wstring &JPname);
	Bone* GetBoneByName(const std::wstring &JPname);
	Bone* GetBoneByENName(const std::wstring &ENname);
	Bone* GetBoneById(uint32_t id);
	Bone* GetRootBone() { return rootBone; }

	void ApplyMorph(const std::wstring &JPname, float weight);
	void ApplyMorph(Morph *morph, float weight);

	struct DebugFlags {
		enum Flags : uint32_t {
			None,
			RenderBones = 0x1,
			RenderJoints = 0x2,
			RenderSoftBodies = 0x4,
			RenderRigidBodies = 0x8,
			DontRenderModel = 0x10,
			DontUpdatePhysics = 0x20,
		};
	};

	DebugFlags::Flags GetDebugFlags() { return (DebugFlags::Flags)m_debugFlags; }
	void SetDebugFlags(DebugFlags::Flags value) { m_debugFlags |= value; }
	void ToggleDebugFlags(DebugFlags::Flags value) { m_debugFlags ^= value; }
	void UnsetDebugFlags(DebugFlags::Flags value) { m_debugFlags &= ~value; }

	Material* GetMaterialById(uint32_t id);
	RenderMaterial* GetRenderMaterialById(uint32_t id);
	std::shared_ptr<RigidBody> GetRigidBodyById(uint32_t id);
	std::shared_ptr<RigidBody> GetRigidBodyByName(const std::wstring &JPname);

	virtual bool Update(float msec);
	virtual void Render(ID3D11DeviceContext *context, std::shared_ptr<Renderer::ViewFrustum> frustum);

	virtual bool LoadModel(const std::wstring &filename);

	void Reset();

#if defined _M_IX86 && defined _MSC_VER
	void *__cdecl operator new(size_t count) {
		return _aligned_malloc(count, 16);
	}

	void __cdecl operator delete(void *object) {
		_aligned_free(object);
	}
#endif

private:
	std::vector<Vertex*> vertices;

	std::vector<uint32_t> verticesIndex;
	std::vector<std::wstring> textures;
	std::vector<Material*> materials;
	std::vector<Bone*> bones;
	std::vector<Morph*> morphs;
	std::vector<Frame*> frames;
	std::vector<SoftBody*> softBodies;
	std::vector<RigidBody*> RigidBodies;

	Bone *rootBone;

	std::vector<RenderMaterial> rendermaterials;

	uint64_t lastpos;

	std::vector<std::shared_ptr<Renderer::Texture>> renderTextures;

	std::shared_ptr<Renderer::D3DRenderer> m_d3d;

	std::wstring basePath;

	static std::vector<std::shared_ptr<Renderer::Texture>> sharedToonTextures;

	std::vector<std::shared_ptr<RigidBody>> m_rigidBodies;
	std::vector<std::shared_ptr<Joint>> m_joints;

	std::vector<PMXShader::VertexType> m_vertices;

	bool updateVertexBuffer(ID3D11DeviceContext *Context);
	bool updateMaterialBuffer(uint32_t material, ID3D11DeviceContext *context);
	bool m_dirtyBuffer;
	ID3D11Buffer *m_materialBuffer;
	ID3D11Buffer *m_vertexBuffer, *m_tmpVertexBuffer;
	ID3D11Buffer *m_indexBuffer;

	uint32_t m_debugFlags;

	std::vector<Bone*> m_prePhysicsBones;
	std::vector<Bone*> m_postPhysicsBones;
	std::vector<Bone*> m_ikBones;

protected:
	virtual bool InitializeBuffers(std::shared_ptr<Renderer::D3DRenderer> d3d);
	virtual void ShutdownBuffers();

	virtual void ReleaseModel();

	virtual bool LoadTexture(ID3D11Device *device);
	virtual void ReleaseTexture();

private:
	void applyVertexMorph(Morph* morph, float weight);
	void applyMaterialMorph(Morph* morph, float weight);
	void applyMaterialMorph(MorphType* morph, RenderMaterial* material, float weight);
	void applyBoneMorph(Morph* morph, float weight);
	void applyFlipMorph(Morph* morph, float weight);
	void applyImpulseMorph(Morph* morph, float weight);

	friend class Loader;
#ifdef PMX_TEST
	friend class PMXTest::BoneTest;
#endif
};

}
