﻿#include "SceneManager.h"
#include "../PMX/PMXModel.h"
#include "../PMX/PMXBone.h"
#include "OBJ/OBJModel.h"
#include "D3DRenderer.h"
#include "../VMD/Motion.h"

#include <iomanip>
#include <sstream>

using Renderer::SceneManager;

SceneManager::SceneManager()
{
}


SceneManager::~SceneManager()
{
	Shutdown();
}

bool SceneManager::Initialize(int width, int height, HWND wnd, std::shared_ptr<Input::Manager> input, std::shared_ptr<Physics::Environment> physics, std::shared_ptr<Dispatcher> dispatcher)
{
	this->input = input;
	this->physics = physics;
	m_dispatcher = dispatcher;

	d3d.reset(new D3DRenderer);
	if (d3d == nullptr)
		return false;

	if (!d3d->Initialize(width, height, VSYNC_ENABLED, wnd, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR)) {
		MessageBox(wnd, L"Failed to initialize Direct3D", L"Error", MB_OK);
		return false;
	}

	m_modelManager.reset(new ModelManager);
	try {
		m_modelManager->loadList();
	}
	catch (std::exception &e) {
		MessageBoxA(wnd, e.what(), "Error", MB_OK);
		return false;
	}

	lightShader.reset(new Shaders::Light);
	if (lightShader == nullptr)
		return false;

	if (!lightShader->InitializeBuffers(d3d->GetDevice(), wnd)) {
		MessageBox(wnd, L"Could not initialize the shader object", L"Error", MB_OK);
		return false;
	}

	textureShader.reset(new Shaders::Texture);
	if (textureShader == nullptr)
		return false;

	if (!textureShader->Initialize(d3d->GetDevice(), wnd)) {
		MessageBox(wnd, L"Could not initialize the texture shader object", L"Error", MB_OK);
		return false;
	}

	m_postProcess.reset(new Shaders::PostProcessEffect);
	if (m_postProcess == nullptr)
		return false;

	if (!m_postProcess->Initialize(d3d->GetDevice(), wnd, width, height)) {
		MessageBox(wnd, L"Could not initialize the effects object", L"Error", MB_OK);
		return false;
	}

	light.reset(new Light);
	if (light == nullptr)
		return false;

	light->SetDirection(-1.0f, -1.0f, 0.0f);
	light->SetSpecularPower(8.f);
	light->SetAmbientColor(1.0f, 1.0f, 1.0f, 1.0f);
	light->SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);

	renderTexture.reset(new D3DTextureRenderer);
	if (renderTexture == nullptr)
		return false;

	if (!renderTexture->Initialize(d3d->GetDevice(), width, height, SCREEN_DEPTH, SCREEN_NEAR))
		return false;

	frustum.reset(new ViewFrustum);
	if (frustum == nullptr)
		return false;

	fullWindow.reset(new OrthoWindowClass);
	if (fullWindow == nullptr)
		return false;

	if (!fullWindow->Initialize(d3d->GetDevice(), width, height))
		return false;

	sky.reset(new SkyBox);
	if (sky == nullptr)
		return false;

	if (!sky->Initialize(d3d, L"./Data/Textures/Sky/violentdays.dds", light, wnd))
		return false;

	m_batch.reset(new DirectX::SpriteBatch(d3d->GetDeviceContext()));
	m_font.reset(new DirectX::SpriteFont(d3d->GetDevice(), L"./Data/Fonts/unifont.spritefont"));

	// Setup bindings
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_Z), [this](void* param) {
		m_models[0]->ApplyMorph(L"ﾍﾞｰﾙ非表示1", 1.0f);
		m_models[0]->ApplyMorph(L"ﾍﾞｰﾙ非表示2", 1.0f);
		m_models[0]->ApplyMorph(L"薔薇非表示", 1.0f);
		m_models[0]->ApplyMorph(L"肩服非表示", 1.0f);
		m_models[0]->ApplyMorph(L"眼帯off", 1.0f);
	});
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyPressed, DIK_A), [this](void* param) {
		static float value = 0.0f;
		static float increment = 0.01f;
		value += increment;

		if (value >= 1.0f) {
			value = 1.0f;
			increment = -0.01f;
		}
		else if (value <= 0.f) {
			value = 0.f;
			increment = 0.01f;
		}

		m_models[0]->ApplyMorph(L"翼羽ばたき", value);
	});
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_S), [this](void* param) {
		m_models[0]->ApplyMorph(L"ウィンク", 0.9f);
	});
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_D), [this](void* param) { for (auto &model : m_models) model->GetRootBone()->Translate(btVector3(1.0f, 0.0f, 0.0f)); });
	//input->AddBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyPressed, DIK_F), [this](void* param) { m_models[0]->GetBoneByName(L"右腕")->Rotate(btVector3(0.0f, 1.0f, 0.0f)); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F), [this](void* param) {
		for (auto &model : m_models) {
			auto bone = model->GetBoneByName(L"右腕");
			DirectX::XMVECTOR direction = dynamic_cast<PMX::detail::BoneImpl*>(bone)->GetOffsetPosition().get128();
			bone->Rotate(btVector3(0, 0, 1), DirectX::XMConvertToRadians(30.0f));
		}
	});
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_C), [this](void* param) {
		for (auto &model : m_models) model->GetBoneByName(L"首")->Rotate(btVector3(0.0f, 1.0f, 0.0f), 0.3f);
	});
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_V), [this](void* param) {
		for (auto model : m_models) {
			auto bone = model->GetBoneByName(L"右腕");
			if (bone) bone->Rotate(btVector3(0.0f, 0.0f, DirectX::XMConvertToRadians(45.0f)));
		}
	});
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnMouseUp, 0), [this](void* param) { m_models[0]->ApplyMorph(L"purple", 1.0f); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyPressed, DIK_G), [this](void* param) {
		for (auto model : m_models) {
			auto body = model->GetRigidBodyByName(L"後髪１");
			if (body) body->getBody()->applyCentralImpulse(btVector3(0, 50, 0));
		}
	});

	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F1), [this](void* param) { for (auto &model : m_models) model->ToggleDebugFlags(PMX::Model::DebugFlags::RenderBones); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F2), [this](void* param) { for (auto &model : m_models) model->ToggleDebugFlags(PMX::Model::DebugFlags::RenderJoints); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F3), [this](void* param) { for (auto &model : m_models) model->ToggleDebugFlags(PMX::Model::DebugFlags::RenderRigidBodies); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F4), [this](void* param) { for (auto &model : m_models) model->ToggleDebugFlags(PMX::Model::DebugFlags::RenderSoftBodies); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F5), [this](void* param) { for (auto &model : m_models) model->ToggleDebugFlags(PMX::Model::DebugFlags::DontRenderModel); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F6), [this](void* param) { for (auto &model : m_models) model->ToggleDebugFlags(PMX::Model::DebugFlags::DontUpdatePhysics); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F7), [this](void* param) { this->physics->pause(); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F8), [this](void* param) { this->physics->hold(); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnKeyUp, DIK_F9), [this](void* param) { this->physics->resume(); });

	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnGamepadDown, XINPUT_GAMEPAD_A), [this](void* param) {for (auto &model : m_models) model->GetBoneByName(L"首")->Rotate(btVector3(0.0f, 1.0f, 0.0f), 0.3f); });
	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnGamepadLeftThumb), [this](void* v) {
		Input::ThumbMovement *value = (Input::ThumbMovement*)v;
		camera->Move(value->dx, 0.0f, value->dy);
		delete v;
	});

	static float rotation[2] = { 0.0f, 0.0f };

	input->addBinding(Input::CallbackInfo(Input::CallbackInfo::OnGamepadRightThumb), [this](void *v) {
		Input::ThumbMovement *value = (Input::ThumbMovement*)v;
		rotation[0] += value->dx / 50.f;
		rotation[1] -= value->dy / 50.f;
		auto rot = DirectX::XMQuaternionRotationRollPitchYaw(rotation[1], rotation[0], 0.0f);
		camera->SetRotation(rot);
		delete v;
	});

	input->setMouseBinding([this](std::shared_ptr<Input::MouseMovement> data) {
		if (data->x != 0 || data->y != 0) {
			rotation[0] += data->x / 1000.f;
			rotation[1] += data->y / 1000.f;
			camera->SetRotation(DirectX::XMQuaternionRotationRollPitchYaw(rotation[1], rotation[0], 0.0f));
		}
	});

	screenWidth = width;
	screenHeight = height;

	this->wnd = wnd;

	return true;
}

bool SceneManager::LoadScene() {
	camera.reset(new Camera(DirectX::XM_PIDIV4, (float)screenWidth / (float)screenHeight, SCREEN_NEAR, SCREEN_DEPTH));
	if (camera == nullptr)
		return false;

	stage.reset(new OBJModel);
	if (!stage->LoadModel(L"./Data/Models/flatground.txt")) {
		MessageBox(wnd, L"Failed to load the stage model", L"Error", MB_OK);
		return false;
	}
	if (!stage->Initialize(d3d, physics)) {
		MessageBox(wnd, L"Failed to initialize the stage object", L"Error", MB_OK);
		return false;
	}
	stage->SetShader(lightShader);

	auto model = m_modelManager->loadModel(L"Tda式ミク・アペンド");
	if (!model) {
		MessageBox(wnd, L"Failed to load the first model", L"Error", MB_OK);
		return false;
	}
	m_models.emplace_back(model);
	if (!model->Initialize(d3d, physics)) {
		MessageBox(wnd, L"Could not initialize the model object", L"Error", MB_OK);
		return false;
	}
	m_pmxShader.emplace_back(new PMX::PMXShader);
	if (!m_pmxShader[0]->InitializeBuffers(d3d->GetDevice(), wnd)) {
		MessageBox(wnd, L"Could not initialize the PMX effects object", L"Error", MB_OK);
		return false;
	}
	model->SetShader(m_pmxShader[0]);
#if 1
	m_models[0]->GetRootBone()->Translate(btVector3(-7.5f, 0, 0));

	model = m_modelManager->loadModel(L"門を開く者 アリス");
	if (!model) {
		MessageBox(wnd, L"Failed to load the second model", L"Error", MB_OK);
		return false;
	}

	m_models.emplace_back(model);

	if (!model->Initialize(d3d, physics)) {
		MessageBox(wnd, L"Could not initialize the model object", L"Error", MB_OK);
		return false;
	}
	m_pmxShader.emplace_back(new PMX::PMXShader);
	if (!m_pmxShader[1]->InitializeBuffers(d3d->GetDevice(), wnd)) {
		MessageBox(wnd, L"Could not initialize the PMX effects object", L"Error", MB_OK);
		return false;
	}
	model->SetShader(m_pmxShader[1]);
	m_models[1]->GetRootBone()->Translate(btVector3(7.5f, 0, 0));
#endif

	MotionManager.reset(new VMD::MotionController);
	assert(MotionManager != nullptr);
#if 0
	auto motion = MotionManager->loadMotion(L"./Data/Musics/rolling girl/camera.vmd");
	//motion->attachCamera(camera);

	motion = MotionManager->loadMotion(L"./Data/Musics/rolling girl/rolling girl.vmd");
	motion->attachModel(m_models[0]);
	motion->attachModel(m_models[1]);
#else
	auto motion = MotionManager->loadMotion(L"./Data/Musics/Yellow/Yellow.vmd");
	motion->attachModel(m_models[0]);
#endif

	camera->SetPosition(0.0f, 10.0f, -30.f);

	lightShader->SetLightCount(1);
	lightShader->SetLights(light->GetAmbientColor(), light->GetDiffuseColor(), light->GetSpecularColor(), light->GetDirection(), DirectX::XMVectorZero(), 0);
	Shaders::Light::MaterialBufferType material;
	material.ambientColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.diffuseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.specularColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	lightShader->UpdateMaterialBuffer(material, d3d->GetDeviceContext());

	for (auto &shader : m_pmxShader) {
		shader->SetLightCount(1);
		shader->SetLights(light->GetAmbientColor(), light->GetDiffuseColor(), light->GetSpecularColor(), light->GetDirection(), DirectX::XMVectorZero(), 0);
	}

	return true;
}

void SceneManager::Shutdown()
{
	if (sky != nullptr) {
		sky->Shutdown();
		sky.reset();
	}

	if (fullWindow != nullptr) {
		fullWindow->Shutdown();
		fullWindow.reset();
	}

	if (renderTexture != nullptr) {
		renderTexture->Shutdown();
		renderTexture.reset();
	}

	light.reset();

	if (lightShader != nullptr) {
		lightShader->Shutdown();
		lightShader.reset();
	}

	if (stage != nullptr) {
		stage->Shutdown();
		stage.reset();
	}

	camera.reset();

	if (d3d != nullptr) {
		d3d->Shutdown();
		d3d.reset();
	}

	physics.reset();
	input.reset();
	m_modelManager.reset();
}

bool SceneManager::Frame(float frameTime)
{
	static float totalTime = 0.0f;

	wchar_t title[512];
	swprintf_s<512>(title, L"XBeat - Frame Time: %.3fms - FPS: %.1f", frameTime, 1000.0f / (float)frameTime);
	SetWindowText(this->wnd, title);

	totalTime += frameTime;

	MotionManager->advanceFrame(frameTime);

#if 0
	DirectX::XMFLOAT3 campos(0,0,0);

	if (input->isKeyPressed(DIK_UPARROW)) {
		if (input->isKeyPressed(DIK_LCONTROL))
			campos.x = 0.1f;
		else if (!input->isKeyPressed(DIK_LSHIFT))
			campos.y = 0.1f;
		else
			campos.z = 0.1f;
	}
	if (input->isKeyPressed(DIK_DOWNARROW)) {
		if (input->isKeyPressed(DIK_LCONTROL))
			campos.x = -0.1f;
		else if (!input->isKeyPressed(DIK_LSHIFT))
			campos.y = -0.1f;
		else
			campos.z = -0.1f;
	}

	camera->Move(campos.x, campos.y, campos.z);
#endif

	camera->update(frameTime);

	if (!Render(frameTime))
		return false;

	return true;
}

bool SceneManager::Render(float frameTime)
{
	if (!RenderToTexture(frameTime))
		return false;

	if (!RenderEffects(frameTime))
		return false;

	if (!Render2DTextureScene(frameTime))
		return false;

	return true;
}

bool SceneManager::RenderToTexture(float frameTime)
{
	renderTexture->SetRenderTarget(d3d->GetDeviceContext(), d3d->GetDepthStencilView());

	renderTexture->ClearRenderTarget(d3d->GetDeviceContext(), d3d->GetDepthStencilView(), 0.0f, 0.0f, 0.0f, 1.0f);

	if (!RenderScene(frameTime))
		return false;

	d3d->SetBackBufferRenderTarget();

	return true;
}

bool SceneManager::RenderScene(float frameTime)
{
	DirectX::XMMATRIX skyView, view, projection, world;

	camera->getViewMatrix(view);
	world = DirectX::XMMatrixIdentity();
	camera->getProjectionMatrix(projection);

	float Distance = camera->getFocalDistance();
	camera->setFocalDistance(0.0f);
	camera->update(0.0f);
	camera->getViewMatrix(skyView);
	camera->setFocalDistance(Distance);

	frustum->Construct(SCREEN_DEPTH, projection, view);

	if (!sky->Render(d3d, world, skyView, projection, camera, light))
		return false;

	lightShader->SetEyePosition(camera->GetPosition());
	lightShader->SetMatrices(world, view, projection);
	if (!lightShader->Update(frameTime, d3d->GetDeviceContext()))
		return false;

	if (!stage->Update(frameTime))
		return false;

	stage->Render(d3d->GetDeviceContext(), frustum);

	for (auto model : m_models) {
		auto shader = std::dynamic_pointer_cast<PMX::PMXShader>(model->GetShader());
		shader->SetEyePosition(camera->GetPosition());
		shader->SetMatrices(world, view, projection);
		if (!shader->Update(frameTime, d3d->GetDeviceContext()))
			return false;
		if (!model->Update(frameTime))
			return false;

		model->Render(d3d->GetDeviceContext(), frustum);
	}

	return true;
}

bool SceneManager::RenderEffects(float frameTime)
{
	DirectX::XMMATRIX view, ortho, world;

	camera->getViewMatrix(view);
	world = DirectX::XMMatrixIdentity();
	d3d->GetOrthoMatrix(ortho);

	if (!m_postProcess->Render(d3d, fullWindow->GetIndexCount(), world, view, ortho, renderTexture, fullWindow, SCREEN_DEPTH, SCREEN_NEAR))
		return false;

	return true;
}

bool SceneManager::Render2DTextureScene(float frameTime)
{
	DirectX::XMMATRIX view, ortho, world;

	camera->getViewMatrix(view);
	world = DirectX::XMMatrixIdentity();
	d3d->GetOrthoMatrix(ortho);

	d3d->SetBackBufferRenderTarget();
	d3d->BeginScene(1.0f, 1.0f, 1.0f, 1.0f);

	d3d->Begin2D();

	fullWindow->Render(d3d->GetDeviceContext());

	if (!textureShader->Render(d3d->GetDeviceContext(), fullWindow->GetIndexCount(), world, view, ortho, m_postProcess->GetCurrentOutputView()))
		return false;

	d3d->End2D();

	// Render UI artifacts
	m_batch->Begin();
	std::wstringstream ss;
	ss.precision(1);
	ss << L"FPS: " << std::fixed << 1000.0f / frameTime << L" - " << frameTime << L"ms";
	m_font->DrawString(m_batch.get(), ss.str().c_str(), DirectX::XMFLOAT2(9.0f, 9.0f), DirectX::Colors::Black, 0, DirectX::XMFLOAT2(0, 0), 1.0f);
	m_font->DrawString(m_batch.get(), ss.str().c_str(), DirectX::XMFLOAT2(11.0f, 9.0f), DirectX::Colors::Black, 0, DirectX::XMFLOAT2(0, 0), 1.0f);
	m_font->DrawString(m_batch.get(), ss.str().c_str(), DirectX::XMFLOAT2(9.0f, 11.0f), DirectX::Colors::Black, 0, DirectX::XMFLOAT2(0, 0), 1.0f);
	m_font->DrawString(m_batch.get(), ss.str().c_str(), DirectX::XMFLOAT2(11.0f, 11.0f), DirectX::Colors::Black, 0, DirectX::XMFLOAT2(0, 0), 1.0f);
	m_font->DrawString(m_batch.get(), ss.str().c_str(), DirectX::XMFLOAT2(10.0f, 10.0f), DirectX::Colors::Yellow, 0, DirectX::XMFLOAT2(0, 0), 1.0f);
	m_batch->End();

	d3d->EndScene();

	return true;
}
