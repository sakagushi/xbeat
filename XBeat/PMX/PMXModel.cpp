﻿#include "../Renderer/Camera.h"
#include "../Renderer/Shaders/LightShader.h"
#include "../Renderer/Light.h"
#include "PMXModel.h"
#include "PMXBone.h"
#include "PMXMaterial.h"
#include "PMXShader.h"
#include "../Renderer/D3DRenderer.h"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <cfloat> // FLT_MIN, FLT_MAX

#include "../Renderer/Model.h"

using namespace std;

using namespace Renderer;

std::vector<std::shared_ptr<Texture>> PMX::Model::sharedToonTextures(0);

//#define EXTENDED_READ

const wchar_t* defaultToonTexs[] = {
	L"./Data/Textures/Toon/toon01.bmp",
	L"./Data/Textures/Toon/toon02.bmp",
	L"./Data/Textures/Toon/toon03.bmp",
	L"./Data/Textures/Toon/toon04.bmp",
	L"./Data/Textures/Toon/toon05.bmp",
	L"./Data/Textures/Toon/toon06.bmp",
	L"./Data/Textures/Toon/toon07.bmp",
	L"./Data/Textures/Toon/toon08.bmp",
	L"./Data/Textures/Toon/toon09.bmp",
	L"./Data/Textures/Toon/toon10.bmp"
};
const int defaultToonTexCount = sizeof (defaultToonTexs) / sizeof (defaultToonTexs[0]);

PMX::Model::Model(void)
{
	m_debugFlags = DebugFlags::None;

	m_indexBuffer = m_vertexBuffer = m_materialBuffer = nullptr;

	rootBone = PMX::Bone::createBone(this, -1, BoneType::Root);
}


PMX::Model::~Model(void)
{
	Shutdown();
}

bool PMX::Model::LoadModel(const wstring &filename)
{
	Loader *loader = new Loader;
	
	if (!loader->loadFromFile(this, filename))
		return false;

	basePath = filename.substr(0, filename.find_last_of(L"\\/") + 1);

	// Initialize the bones
	rootBone->initialize(nullptr);
	for (uint32_t Id = 0; Id < loader->Bones.size(); ++Id) {
		auto Bone = Bone::createBone(this, Id, (loader->Bones[Id].Flags & (uint16_t)BoneFlags::IK) != 0 ? BoneType::IK : BoneType::Regular);
		bones.push_back(Bone);
	}

	for (auto &Bone : bones) {
		Bone->initialize(&loader->Bones[Bone->getId()]);
		if (Bone->hasAnyFlag((uint16_t)BoneFlags::PostPhysicsDeformation))
			m_postPhysicsBones.push_back(Bone);
		else
			m_prePhysicsBones.push_back(Bone);

		if (Bone->isIK()) {
			m_ikBones.push_back(Bone);
		}
	}

	auto sortFn = [](Bone* a, Bone* b) {
		return a->getDeformationOrder() < b->getDeformationOrder() || (a->getDeformationOrder() == b->getDeformationOrder() && a->getId() < b->getId());
	};

	std::sort(m_prePhysicsBones.begin(), m_prePhysicsBones.end(), sortFn);
	std::sort(m_postPhysicsBones.begin(), m_postPhysicsBones.end(), sortFn);

	// Initialize the rigid bodies
	for (auto &Body : loader->RigidBodies) {
		std::shared_ptr<RigidBody> RigidBody(new RigidBody);
		assert(RigidBody != nullptr);
		m_rigidBodies.emplace_back(RigidBody);

		RigidBody->Initialize(m_physics, this, &Body);
	}

	// Initialize the constraints
	for (auto &Joint : loader->Joints) {
		std::shared_ptr<PMX::Joint> Constraint(new PMX::Joint);
		assert(Constraint != nullptr);
		m_joints.emplace_back(Constraint);

		Constraint->Initialize(m_physics, this, &Joint);
	}

	// Initialize the soft bodies
	for (auto &body : softBodies)
	{
		body->Create(m_physics, this);
	}

	return true;
}

void PMX::Model::ReleaseModel()
{
	for (std::vector<PMX::Vertex*>::size_type i = 0; i < vertices.size(); i++) {
		delete vertices[i];
		vertices[i] = nullptr;
	}
	vertices.clear();
	vertices.shrink_to_fit();

	verticesIndex.clear();
	verticesIndex.shrink_to_fit();
	
	textures.shrink_to_fit();

	for (std::vector<PMX::Material*>::size_type i = 0; i < materials.size(); i++) {
		delete materials[i];
		materials[i] = nullptr;
	}
	materials.shrink_to_fit();

	for (std::vector<PMX::Bone*>::size_type i = 0; i < bones.size(); i++) {
		delete bones[i];
		bones[i] = nullptr;
	}
	bones.shrink_to_fit();
	m_prePhysicsBones.clear();
	m_postPhysicsBones.clear();

	for (std::vector<PMX::Morph*>::size_type i = 0; i < morphs.size(); i++) {
		delete morphs[i];
		morphs[i] = nullptr;
	}
	morphs.shrink_to_fit();

	for (std::vector<PMX::Frame*>::size_type i = 0; i < frames.size(); i++) {
		delete frames[i];
		frames[i] = nullptr;
	}
	frames.shrink_to_fit();

	m_rigidBodies.clear();
	m_rigidBodies.shrink_to_fit();

	for (auto &joint : m_joints) {
		joint->Shutdown(m_physics);
	}
	m_joints.clear();
	m_joints.shrink_to_fit();

	for (std::vector<PMX::SoftBody*>::size_type i = 0; i < softBodies.size(); i++) {
		delete softBodies[i];
		softBodies[i] = nullptr;
	}
	softBodies.shrink_to_fit();

	m_vertices.clear();
	m_vertices.shrink_to_fit();
}

DirectX::XMFLOAT4 color4ToFloat4(const PMX::Color4 &c) 
{
	return DirectX::XMFLOAT4(c.red, c.green, c.blue, c.alpha);
}

bool PMX::Model::InitializeBuffers(std::shared_ptr<Renderer::D3DRenderer> d3d)
{
	if (d3d == nullptr)
		return true; // Exit silently...?

	m_d3d = d3d;

	ID3D11Device *device = d3d->GetDevice();

	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc, materialBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	materialBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	materialBufferDesc.ByteWidth = sizeof (Shaders::Light::MaterialBufferType);
	materialBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	materialBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	materialBufferDesc.MiscFlags = 0;
	materialBufferDesc.StructureByteStride = 0;

	result = device->CreateBuffer(&materialBufferDesc, NULL, &m_materialBuffer);
	if (FAILED(result))
		return false;

	this->rendermaterials.resize(this->materials.size());

	uint32_t lastIndex = 0;

	std::vector<UINT> idx;
	DirectX::XMFLOAT4 boneWeights;
	DirectX::XMUINT4 boneIndices;

	for (uint32_t k = 0; k < this->rendermaterials.size(); k++) {
		rendermaterials[k].startIndex = lastIndex;

		for (uint32_t i = 0; i < (uint32_t)this->materials[k]->indexCount; i++) {
			Vertex* vertex = vertices[this->verticesIndex[i + lastIndex]];
			vertex->materials.push_front(std::pair<RenderMaterial*,UINT>(&rendermaterials[k], i));

			switch (vertex->weightMethod) {
			case VertexWeightMethod::BDEF1:
				boneWeights = DirectX::XMFLOAT4(vertex->boneInfo.BDEF.weights[0], 0, 0, 0);
				boneIndices = DirectX::XMUINT4(vertex->boneInfo.BDEF.boneIndexes[0], 0, 0, 0);
				break;
			case VertexWeightMethod::BDEF2:
				boneWeights = DirectX::XMFLOAT4(vertex->boneInfo.BDEF.weights[0], vertex->boneInfo.BDEF.weights[1], 0, 0);
				boneIndices = DirectX::XMUINT4(vertex->boneInfo.BDEF.boneIndexes[0], vertex->boneInfo.BDEF.boneIndexes[1], 0, 0);
				break;
			case VertexWeightMethod::QDEF:
			case VertexWeightMethod::BDEF4:
				boneWeights = DirectX::XMFLOAT4(vertex->boneInfo.BDEF.weights[0], vertex->boneInfo.BDEF.weights[1], vertex->boneInfo.BDEF.weights[2], vertex->boneInfo.BDEF.weights[3]);
				boneIndices = DirectX::XMUINT4(vertex->boneInfo.BDEF.boneIndexes[0], vertex->boneInfo.BDEF.boneIndexes[1], vertex->boneInfo.BDEF.boneIndexes[2], vertex->boneInfo.BDEF.boneIndexes[3]);
				break;
			case VertexWeightMethod::SDEF:
				boneWeights = DirectX::XMFLOAT4(vertex->boneInfo.SDEF.weightBias, 1.0f - vertex->boneInfo.SDEF.weightBias, 0, 0);
				boneIndices = DirectX::XMUINT4(vertex->boneInfo.SDEF.boneIndexes[0], vertex->boneInfo.SDEF.boneIndexes[1], 0, 0);
				break;
			}
			m_vertices.emplace_back(PMXShader::VertexType{
				DirectX::XMFLOAT3(vertex->position.x, vertex->position.y, vertex->position.z),
				DirectX::XMFLOAT3(vertex->normal.x, vertex->normal.y, vertex->normal.z),
				DirectX::XMFLOAT2(vertex->uv[0], vertex->uv[1]),
				/*{
					vertex->uvEx[0].get128(),
					vertex->uvEx[1].get128(),
					vertex->uvEx[2].get128(),
					vertex->uvEx[3].get128()
				},*/
				boneIndices,
				boneWeights,
				k,
			});

			idx.emplace_back(i);
		}

		lastIndex += this->materials[k]->indexCount;

		rendermaterials[k].dirty |= RenderMaterial::DirtyFlags::Textures;
		rendermaterials[k].materialIndex = k;
		rendermaterials[k].indexCount = this->materials[k]->indexCount;
	}

	// Initialize bone buffers
	for (auto &bone : bones) {
		bone->initializeDebug(d3d->GetDeviceContext());
	}

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = (UINT)(sizeof(PMXShader::VertexType) * m_vertices.size());
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	vertexData.pSysMem = m_vertices.data();
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	if (FAILED(result))
		return false;

	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_tmpVertexBuffer);
	if (FAILED(result))
		return false;

	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = (UINT)(sizeof(UINT) * idx.size());
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	indexData.pSysMem = idx.data();
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	if (FAILED(result))
		return false;

#ifdef DEBUG
	m_tmpVertexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, 6, "PMX SO");
	m_vertexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, 6, "PMX VB");
	m_indexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, 6, "PMX IB");
#endif

	// Initialize rigid body debug info
	for (auto &Body : m_rigidBodies) {
		Body->InitializeDebug(d3d->GetDeviceContext());
	}

	// Initialize joints debug info
	for (auto &Joint : m_joints) {
		Joint->InitializeDebug(d3d->GetDeviceContext());
	}

	return true;
}

void PMX::Model::ShutdownBuffers()
{
	DX_DELETEIF(m_materialBuffer);
	DX_DELETEIF(m_tmpVertexBuffer);
	DX_DELETEIF(m_vertexBuffer);
	DX_DELETEIF(m_indexBuffer);

	for (auto &material : rendermaterials)
	{
		material.Shutdown();
	}

	rendermaterials.resize(0);
}

bool PMX::Model::updateMaterialBuffer(uint32_t material, ID3D11DeviceContext *context)
{
	ID3D11ShaderResourceView *textures[3];
	auto shader = std::dynamic_pointer_cast<PMXShader>(m_shader);
	if (!shader) return false;

	auto &mbuffer = shader->GetMaterial(material);

	textures[0] = rendermaterials[material].baseTexture ? rendermaterials[material].baseTexture->GetTexture() : nullptr;
	textures[1] = rendermaterials[material].sphereTexture ? rendermaterials[material].sphereTexture->GetTexture() : nullptr;
	textures[2] = rendermaterials[material].toonTexture ? rendermaterials[material].toonTexture->GetTexture() : nullptr;

	mbuffer.flags = 0;

	mbuffer.flags |= (textures[1] == nullptr || materials[material]->sphereMode == MaterialSphereMode::Disabled) ? 0x01 : 0;
	mbuffer.flags |= materials[material]->sphereMode == MaterialSphereMode::Add ? 0x02 : 0;
	mbuffer.flags |= textures[2] == nullptr ? 0x04 : 0;
	mbuffer.flags |= textures[0] == nullptr ? 0x08 : 0;

	mbuffer.morphWeight = rendermaterials[material].getWeight();

	mbuffer.addBaseCoefficient = color4ToFloat4(rendermaterials[material].getAdditiveMorph()->baseCoefficient);
	mbuffer.addSphereCoefficient = color4ToFloat4(rendermaterials[material].getAdditiveMorph()->sphereCoefficient);
	mbuffer.addToonCoefficient = color4ToFloat4(rendermaterials[material].getAdditiveMorph()->toonCoefficient);
	mbuffer.mulBaseCoefficient = color4ToFloat4(rendermaterials[material].getMultiplicativeMorph()->baseCoefficient);
	mbuffer.mulSphereCoefficient = color4ToFloat4(rendermaterials[material].getMultiplicativeMorph()->sphereCoefficient);
	mbuffer.mulToonCoefficient = color4ToFloat4(rendermaterials[material].getMultiplicativeMorph()->toonCoefficient);

	mbuffer.specularColor = rendermaterials[material].getSpecular(materials[material]);

	if ((mbuffer.flags & 0x04) == 0) {
		mbuffer.diffuseColor = rendermaterials[material].getDiffuse(materials[material]);
		mbuffer.ambientColor = rendermaterials[material].getAmbient(materials[material]);
	}
	else {
		mbuffer.diffuseColor = mbuffer.ambientColor = rendermaterials[material].getAverage(materials[material]);
	}
	mbuffer.index = (int)material;

	return true;
}

bool PMX::Model::updateVertexBuffer(ID3D11DeviceContext *Context)
{
	bool Touched = false;

	for (auto &Material : rendermaterials) {
		if ((Material.dirty & RenderMaterial::DirtyFlags::VertexBuffer) != 0) {
			Material.dirty &= ~RenderMaterial::DirtyFlags::VertexBuffer;

			for (uint32_t i = Material.startIndex; i < Material.indexCount + Material.startIndex; ++i) {
				auto Vertex = this->vertices[this->verticesIndex[i]];
				DirectX::XMVECTOR Position = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&Vertex->position), Vertex->MorphOffset);
				DirectX::XMStoreFloat3(&m_vertices[i].position, Position);
			}

			Touched = true;
		}
	}

	if (!Touched) return true;

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	HRESULT Result = Context->Map(m_tmpVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(Result))
		return false;

	memcpy(MappedResource.pData, m_vertices.data(), m_vertices.size() * sizeof(PMX::PMXShader::VertexType));

	Context->Unmap(m_tmpVertexBuffer, 0);

	Context->CopyResource(m_vertexBuffer, m_tmpVertexBuffer);

	return true;
}

bool PMX::Model::Update(float msec)
{
	if ((m_debugFlags & DebugFlags::DontUpdatePhysics) == 0) {
		for (auto &bone : m_prePhysicsBones) {
			bone->update();
		}

		for (auto &body : m_rigidBodies) {
			body->Update();
		}

		for (auto &bone : m_postPhysicsBones) {
			bone->update();
		}

		for (auto &bone : m_ikBones) {
			bone->performIK();
		}
	}

	return true;
}

void PMX::Model::Reset()
{
	for (auto &morph : morphs) {
		ApplyMorph(morph, 0.0f);
	}

	for (auto &bone : bones) {
		bone->clearIK();
		bone->resetTransform();
	}
}

void PMX::Model::Render(ID3D11DeviceContext *context, std::shared_ptr<ViewFrustum> frustum)
{
#ifdef DEBUG
	if ((m_debugFlags & DebugFlags::DontRenderModel) == 0)
#endif
	{
		unsigned int stride = sizeof(PMXShader::VertexType);

		ID3D11ShaderResourceView *textures[3];

		unsigned int vsOffset = 0;

		context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto shader = std::dynamic_pointer_cast<PMXShader>(m_shader);

		for (auto & bone : bones) {
			auto &shaderBone = shader->GetBone(bone->getId());
			shaderBone.position = bone->getStartPosition().get128();
			auto t = bone->getSkinningTransform();
			shaderBone.transform = DirectX::XMMatrixTranspose(DirectX::XMMatrixAffineTransformation(DirectX::XMVectorSplatOne(), bone->getStartPosition().get128(), t.getRotation().get128(), t.getOrigin().get128()));
		}

		shader->UpdateBoneBuffer(context);

		updateVertexBuffer(context);

		context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &vsOffset);

		for (uint32_t i = 0; i < rendermaterials.size(); i++) {
			if (!updateMaterialBuffer(i, context))
				return;
		}

		shader->UpdateMaterialBuffer(context);
		shader->PrepareRender(context);

		m_d3d->EnableAlphaBlending();
		context->RSSetState(m_d3d->GetRasterState(1));
		
		for (uint32_t i = 0; i < rendermaterials.size(); i++) {
			textures[0] = rendermaterials[i].baseTexture ? rendermaterials[i].baseTexture->GetTexture() : nullptr;
			textures[1] = rendermaterials[i].sphereTexture ? rendermaterials[i].sphereTexture->GetTexture() : nullptr;
			textures[2] = rendermaterials[i].toonTexture ? rendermaterials[i].toonTexture->GetTexture() : nullptr;

			context->PSSetShaderResources(0, 3, textures);
			m_shader->Render(context, rendermaterials[i].indexCount, rendermaterials[i].startIndex);
		}
		
		context->RSSetState(m_d3d->GetRasterState(0));
		m_d3d->DisableAlphaBlending();
	}

	DirectX::XMMATRIX view = DirectX::XMMatrixTranspose(m_shader->GetCBuffer().matrix.view);
	DirectX::XMMATRIX projection = DirectX::XMMatrixTranspose(m_shader->GetCBuffer().matrix.projection);
	DirectX::XMMATRIX world = DirectX::XMMatrixRotationQuaternion(rootBone->getTransform().getRotation().get128()) * DirectX::XMMatrixTranslationFromVector(rootBone->getTransform().getOrigin().get128());

#ifdef DEBUG
	context->RSSetState(m_d3d->GetRasterState(1));

	if (m_debugFlags & DebugFlags::RenderJoints) {
		for (auto &joint : m_joints) {
			joint->Render(view, projection);
		}
	}

	if (m_debugFlags & DebugFlags::RenderRigidBodies) {
		for (auto &body : m_rigidBodies) {
			body->Render(world, view, projection);
		}
	}

	if (m_debugFlags & DebugFlags::RenderBones) {
		for (auto &bone : bones)
			bone->render(world, view, projection);
	}

	context->RSSetState(m_d3d->GetRasterState(0));
#endif
}

bool PMX::Model::LoadTexture(ID3D11Device *device)
{
	if (device == nullptr)
		return true; // Exit silently... ?

	// Initialize default toon textures, if they are not loaded
	if (sharedToonTextures.size() != defaultToonTexCount) {
		sharedToonTextures.resize(defaultToonTexCount);

		// load each default toon texture
		for (int i = 0; i < defaultToonTexCount; i++) {
			sharedToonTextures[i].reset(new Texture);

			if (!sharedToonTextures[i]->Initialize(device, defaultToonTexs[i]))
				return false;
		}
	}

	// Initialize each specificied texture
	renderTextures.resize(textures.size());
	for (uint32_t i = 0; i < renderTextures.size(); i++) {
		renderTextures[i].reset(new Texture);

		if (!renderTextures[i]->Initialize(device, basePath + textures[i])) {
			renderTextures[i].reset();
		}
	}

	// Assign the textures to each material
	for (uint32_t i = 0; i < rendermaterials.size(); i++) {
		bool hasSphere = materials[i]->sphereMode != MaterialSphereMode::Disabled;

		if (materials[i]->baseTexture < textures.size()) {
			rendermaterials[i].baseTexture = renderTextures[materials[i]->baseTexture];
		}

		if (hasSphere && materials[i]->sphereTexture < textures.size()) {
			rendermaterials[i].sphereTexture = renderTextures[materials[i]->sphereTexture];
		}

		if (materials[i]->toonFlag == MaterialToonMode::CustomTexture && materials[i]->toonTexture.custom < textures.size()) {
			rendermaterials[i].toonTexture = renderTextures[materials[i]->toonTexture.custom];
		}
		else if (materials[i]->toonFlag == MaterialToonMode::DefaultTexture && materials[i]->toonTexture.default < defaultToonTexCount) {
			rendermaterials[i].toonTexture = sharedToonTextures[materials[i]->toonTexture.default];			
		}
	}

	return true;
}

void PMX::Model::ReleaseTexture()
{
	renderTextures.clear();
}

PMX::Bone* PMX::Model::GetBoneByName(const std::wstring &JPname)
{
	for (auto bone : bones) {
		if (bone->getName().japanese.compare(JPname) == 0)
			return bone;
	}

	return nullptr;
}

PMX::Bone* PMX::Model::GetBoneByENName(const std::wstring &ENname)
{
	for (auto bone : bones) {
		if (bone->getName().english.compare(ENname) == 0)
			return bone;
	}

	return nullptr;
}

PMX::Bone* PMX::Model::GetBoneById(uint32_t id)
{
	if (id == -1)
		return rootBone;

	if (id >= bones.size())
		return nullptr;

	return bones[id];
}

PMX::Material* PMX::Model::GetMaterialById(uint32_t id)
{
	if (id == -1 || id >= materials.size())
		return nullptr;

	return materials[id];
}

PMX::RenderMaterial* PMX::Model::GetRenderMaterialById(uint32_t id)
{
	if (id == -1 || id >= rendermaterials.size())
		return nullptr;

	return &rendermaterials[id];
}

std::shared_ptr<PMX::RigidBody> PMX::Model::GetRigidBodyById(uint32_t id)
{
	if (id == -1 || id >= m_rigidBodies.size())
		return nullptr;

	return m_rigidBodies[id];
}

std::shared_ptr<PMX::RigidBody> PMX::Model::GetRigidBodyByName(const std::wstring &JPname)
{
	for (auto body : m_rigidBodies) {
		if (body->GetName().japanese.compare(JPname) == 0)
			return body;
	}

	return nullptr;
}

void PMX::Model::ApplyMorph(const std::wstring &nameJP, float weight)
{
	for (auto morph : morphs) {
		if (morph->name.japanese.compare(nameJP) == 0) {
			ApplyMorph(morph, weight);
			break;
		}
	}
}

void PMX::Model::ApplyMorph(Morph *morph, float weight)
{
	// 0.0 <= weight <= 1.0
	if (weight <= 0.0f)
		weight = 0.0f;
	else if (weight >= 1.0f)
		weight = 1.0f;

	// Do the work only if we have a different weight from before
	if (morph->appliedWeight == weight)
		return;

	morph->appliedWeight = weight;

	switch (morph->type) {
	case MorphType::Group:
		for (auto i : morph->data) {
			if (i.group.index < morphs.size() && i.group.index >= 0) {
				ApplyMorph(morphs[i.group.index], i.group.rate * weight);
			}
		}
		break;
	case MorphType::Vertex:
		applyVertexMorph(morph, weight);
		break;
	case MorphType::Bone:
		applyBoneMorph(morph, weight);
		break;
	case MorphType::Material:
		applyMaterialMorph(morph, weight);
		break;
	case MorphType::UV:
	case MorphType::UV1:
	case MorphType::UV2:
	case MorphType::UV3:
	case MorphType::UV4:
		break;
	case MorphType::Flip:
		applyFlipMorph(morph, weight);
		break;
	case MorphType::Impulse:
		applyImpulseMorph(morph, weight);
		break;
	}
}

void PMX::Model::applyVertexMorph(Morph *morph, float weight)
{
	for (auto i : morph->data) {
		Vertex *v = vertices[i.vertex.index];

		auto it = ([morph, v]() { for (auto i = v->morphs.begin(); i != v->morphs.end(); i++) if ((*i)->morph == morph) return i; return v->morphs.end(); })();
		if (it == v->morphs.end()) {
			if (weight == 0.0f)
				continue;
			else {
				Vertex::MorphData *md = new Vertex::MorphData;
				md->morph = morph; md->type = &i; md->weight = weight;
				v->morphs.push_back(md);
			}
		}
		else {
			if (weight == 0.0f)
				v->morphs.erase(it);
			else
				(*it)->weight = weight;
		}

		v->MorphOffset = DirectX::XMVectorZero();
		for (auto &m : v->morphs) {
			v->MorphOffset = v->MorphOffset + DirectX::XMVectorSet(m->type->vertex.offset[0] * m->weight, m->type->vertex.offset[1] * m->weight, m->type->vertex.offset[2] * m->weight, 0.0f);
		}

		// Mark the material for update next frame
		for (auto &m : v->materials)
			m.first->dirty |= RenderMaterial::DirtyFlags::VertexBuffer;
	}
}

void PMX::Model::applyBoneMorph(Morph *morph, float weight)
{
	for (auto i : morph->data) {
		Bone *bone = bones[i.bone.index];
		bone->applyMorph(morph, weight);
	}
}

void PMX::Model::applyMaterialMorph(Morph *morph, float weight)
{
	for (auto i : morph->data) {
		// If target is -1, apply morph to all materials
		if (i.material.index == -1) {
			for (auto m : rendermaterials)
				applyMaterialMorph(&i, &m, weight);
			continue;
		}

		assert("Material morph index out of range" && i.material.index < rendermaterials.size());
		applyMaterialMorph(&i, &rendermaterials[i.material.index], weight);
	}
}

void PMX::Model::applyMaterialMorph(MorphType *morph, PMX::RenderMaterial *material, float weight)
{
	material->ApplyMorph(&morph->material, weight);

	m_dirtyBuffer = true;
}

void PMX::Model::applyFlipMorph(Morph* morph, float weight)
{
	// Flip morph is like a group morph, but it is only applied to a single index, while all the others are set to 0
	int index = (int)((morph->data.size() + 1) * weight) - 1;

	for (uint32_t i = 0; i < morph->data.size(); i++) {
		if (i == index)
			ApplyMorph(morphs[morph->data[i].group.index], morph->data[i].group.rate);
		else
			ApplyMorph(morphs[morph->data[i].group.index], 0.0f);
	}
}

void PMX::Model::applyImpulseMorph(Morph* morph, float weight)
{
	for (auto &Morph : morph->data) {
		assert("Impulse morph rigid body index out of range" && Morph.impulse.index < m_rigidBodies.size());
		auto Body = m_rigidBodies[Morph.impulse.index];
		assert("Impulse morph cannot be applied to a kinematic rigid body" && Body->isDynamic());
		Body->getBody()->applyCentralImpulse(btVector3(Morph.impulse.velocity[0], Morph.impulse.velocity[1], Morph.impulse.velocity[2]));
		Body->getBody()->applyTorqueImpulse(btVector3(Morph.impulse.rotationTorque[0], Morph.impulse.rotationTorque[1], Morph.impulse.rotationTorque[2]));;
	}
}
