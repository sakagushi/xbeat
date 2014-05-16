#include "MMDEffect.h"

#include <D3Dcompiler.h>
#include <fstream>

using Renderer::Shaders::MMDEffect;


MMDEffect::MMDEffect(void)
{
	m_blurAmmount = 3.8f;
}


MMDEffect::~MMDEffect(void)
{
	Shutdown();
}


bool MMDEffect::Initialize(ID3D11Device *device, HWND wnd, int width, int height)
{
	if (!InitializeEffect(device, wnd, width, height, L"./Data/Shaders/MMD.fx"))
		return false;

	return true;
}

void MMDEffect::Shutdown()
{
	ShutdownEffect();
}

bool MMDEffect::Render(std::shared_ptr<Renderer::D3DRenderer> d3d, int indexCount, DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, std::shared_ptr<Renderer::D3DTextureRenderer> renderTexture, std::shared_ptr<Renderer::OrthoWindowClass> window, float farZ, float nearZ)
{
	ID3D11DeviceContext *context = d3d->GetDeviceContext();

	//RenderEffect(context, indexCount);

	bufferIndex = 0;
	renderTexture->CopyIntoTexture(context, &m_originalBackTexture);
	renderTexture->CopyIntoTexture(context, &m_currentBackTexture);

	renderTexture->SetRenderTarget(context, NULL);
	d3d->Begin2D();

	context->IASetInputLayout(m_layout);

	m_originalSceneVar->AsShaderResource()->SetResource(m_originalBackView);
	m_depthSceneVar->AsShaderResource()->SetResource(d3d->GetDepthResourceView());
	m_currentSceneVar->AsShaderResource()->SetResource(m_currentBackView);
	m_defaultSamplerVar->AsSampler()->SetSampler(0, m_sampler);

	if (!SetEffectParameters(context, world, view, projection, farZ, nearZ))
		return false;

	for (uint32_t group = 0; group < m_numGroups; group++) 
	{
		for (uint32_t technique = 0; technique < m_numTechniques[group]; technique++)
		{
			for (uint32_t pass = 0; pass < m_numPasses[group][technique]; pass++)
			{
				//bufferIndex++;

				window->Render(context);

				m_effect->GetGroupByIndex(group)->GetTechniqueByIndex(technique)->GetPassByIndex(pass)->Apply(0, context);

				context->DrawIndexed(indexCount, 0, 0);

				renderTexture->CopyIntoTexture(context, &m_currentBackTexture);
			}
		}
	}

	ID3D11ShaderResourceView *nulls[3] = { NULL, NULL, NULL };
	context->PSSetShaderResources(0, 3, nulls);

	d3d->End2D();
	d3d->ResetViewport();
	d3d->SetBackBufferRenderTarget();

	return true;
}

bool MMDEffect::InitializeEffect(ID3D11Device *device, HWND wnd, int width, int height, const std::wstring &filename)
{
	HRESULT result;
	ID3D10Blob *errorMsg;
	D3D11_BUFFER_DESC matrixBufferDesc;
	D3D11_INPUT_ELEMENT_DESC layoutDesc[2];
	D3D11_SAMPLER_DESC samplerDesc;
	D3D11_TEXTURE2D_DESC texDesc;
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;

	ZeroMemory(&texDesc, sizeof (D3D11_TEXTURE2D_DESC));

	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	result = device->CreateTexture2D(&texDesc, NULL, &m_originalBackTexture);
	if (FAILED(result))
		return false;

	ZeroMemory(&texDesc, sizeof (D3D11_TEXTURE2D_DESC));

	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	result = device->CreateTexture2D(&texDesc, NULL, &m_currentBackTexture);
	if (FAILED(result))
		return false;

	shaderResourceViewDesc.Format = texDesc.Format;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;

	result = device->CreateShaderResourceView(m_originalBackTexture, &shaderResourceViewDesc, &m_originalBackView);
	if (FAILED(result))
		return false;

	result = device->CreateShaderResourceView(m_currentBackTexture, &shaderResourceViewDesc, &m_currentBackView);
	if (FAILED(result))
		return false;

#ifdef _DEBUG
	result = D3DX11CompileEffectFromFile(filename.c_str(), NULL, NULL, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, device, &m_effect, &errorMsg);
#else
	result = D3DX11CompileEffectFromFile(filename.c_str(), NULL, NULL, 0, 0, device, &m_effect, &errorMsg);
#endif
	if (FAILED(result)) {
		if (errorMsg == NULL) {
			MessageBox(wnd, (std::wstring(L"Shader file not found: ") + filename).c_str(), L"Error", MB_ICONERROR | MB_OK);
			return false;
		}
		else {
			OutputErrorMessage(errorMsg, wnd, filename);
			return false;
		}
	}

	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	for (auto &i : samplerDesc.BorderColor)
		i = 0.0f;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	result = device->CreateSamplerState(&samplerDesc, &m_sampler);
	if (FAILED(result))
		return false;

	// Create vertex shader input layout
	layoutDesc[0].SemanticName = "POSITION";
	layoutDesc[0].SemanticIndex = 0;
	layoutDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layoutDesc[0].InputSlot = 0;
	layoutDesc[0].AlignedByteOffset = 0;
	layoutDesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layoutDesc[0].InstanceDataStepRate = 0;

	layoutDesc[1].SemanticName = "TEXCOORD";
	layoutDesc[1].SemanticIndex = 0;
	layoutDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	layoutDesc[1].InputSlot = 0;
	layoutDesc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	layoutDesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layoutDesc[1].InstanceDataStepRate = 0;

	// Create matrix buffer
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.ByteWidth = sizeof (MatrixBuffer);
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;

	result = device->CreateBuffer(&matrixBufferDesc, NULL, &m_matrixBuffer);
	if (FAILED(result))
		return false;

	matrixBufferDesc.ByteWidth = sizeof (DOFBuffer);

	result = device->CreateBuffer(&matrixBufferDesc, NULL, &m_dofBuffer);
	if (FAILED(result))
		return false;

	matrixBufferDesc.ByteWidth = sizeof (BlurSamplersBuffer);

	result = device->CreateBuffer(&matrixBufferDesc, NULL, &m_blurBuffer);
	if (FAILED(result))
		return false;

	matrixBufferDesc.ByteWidth = sizeof (ScreenSizeBuffer);
	matrixBufferDesc.CPUAccessFlags = 0;
	matrixBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;

	ScreenSizeBuffer ssb;
	ssb.dimentions = DirectX::XMFLOAT2((float)width, (float)height);
	ssb.texelSize = DirectX::XMFLOAT2(1.0f / texDesc.Width, 1.0f / texDesc.Height);
	D3D11_SUBRESOURCE_DATA srd;
	srd.pSysMem = (LPVOID)&ssb;
	srd.SysMemPitch = 0;
	srd.SysMemSlicePitch = 0;
	result = device->CreateBuffer(&matrixBufferDesc, &srd, &m_screenSizeBuffer);
	if (FAILED(result))
		return false;

	// Parse the effect info
	D3DX11_GROUP_DESC groupDesc;
	D3DX11_EFFECT_DESC effectDesc;
	D3DX11_TECHNIQUE_DESC techniqueDesc;
	D3DX11_PASS_DESC passDesc;

	result = m_effect->GetDesc(&effectDesc);
	if (FAILED(result))
		return false;

	m_numGroups = effectDesc.Groups;
	m_numTechniques = new uint32_t[m_numGroups];
	m_numPasses = new uint32_t*[m_numGroups];

	ID3DX11EffectGroup *group;
	ID3DX11EffectTechnique *technique;
	ID3DX11EffectPass *pass;

	for (uint32_t g = 0; g < m_numGroups; g++) {
		group = m_effect->GetGroupByIndex(g);
		if (group == NULL)
			return false;
		result = group->GetDesc(&groupDesc);
		if (FAILED(result))
			return false;

		m_numTechniques[g] = groupDesc.Techniques;
		m_numPasses[g] = new uint32_t[m_numTechniques[g]];

		for (uint32_t n = 0; n < m_numTechniques[g]; n++) {
			technique = group->GetTechniqueByIndex(n);
			if (technique == NULL)
				return false;
			result = technique->GetDesc(&techniqueDesc);
			if (FAILED(result))
				return false;

			m_numPasses[g][n] = techniqueDesc.Passes;

			// Only the first pass has a vertex shader
			pass = technique->GetPassByIndex(0);
			if (pass == NULL)
				return false;
			result = pass->GetDesc(&passDesc);
			if (FAILED(result))
				return false;
			result = device->CreateInputLayout(layoutDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &m_layout);
			if (FAILED(result))
				return false;
		}
	}

	// Initialize variables pointers
	m_blurVar = m_effect->GetConstantBufferByName("GaussianBlurBuffer");
	m_dofVar = m_effect->GetConstantBufferByName("dofbuffer");
	m_screenSizeVar = m_effect->GetConstantBufferByName("ScreenSize");
	m_WVPVar = m_effect->GetConstantBufferByName("WVP");
	m_defaultSamplerVar = m_effect->GetVariableByName("TexSampler");
	m_currentSceneVar = m_effect->GetVariableByName("CurrentScene");
	m_depthSceneVar = m_effect->GetVariableByName("DepthScene");
	m_originalSceneVar = m_effect->GetVariableByName("OriginalScene");

	return true;
}

void MMDEffect::ShutdownEffect()
{
	if (m_numPasses != nullptr) {
		delete[] m_numPasses;
		m_numPasses = nullptr;
	}

	if (m_matrixBuffer) {
		m_matrixBuffer = nullptr;
	}

	if (m_sampler) {
		m_sampler = nullptr;
	}

	if (m_layout) {
		m_layout = nullptr;
	}

	if (m_effect) {
		m_effect = nullptr;
	}
}

void MMDEffect::OutputErrorMessage(ID3D10Blob *errorMessage, HWND wnd, const std::wstring &file)
{
	char *compileError;
	SIZE_T bufferSize;
	std::ofstream fout;

	compileError = (char*)errorMessage->GetBufferPointer();
	bufferSize = errorMessage->GetBufferSize();

	fout.open("shader-errors.log", std::ios::app);
	if (fout.bad()) {
		errorMessage->Release();
		return;
	}

	fout.write(compileError, bufferSize);

	fout.close();

	errorMessage->Release();

	MessageBox(wnd, L"Error compiling effect", file.c_str(), MB_OK);
}

bool MMDEffect::SetEffectParameters(ID3D11DeviceContext *context, DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, float farZ, float nearZ)
{
	static auto GaussianFunction = [](float sigmaSquared, float offset) { return (1.0f / sqrtf(2.0f * DirectX::XM_PI * sigmaSquared)) * expf(-(offset * offset) / (2 * sigmaSquared)); };
	BlurSamplersBuffer *blurBuffer;
	MatrixBuffer *matrixBuffer;
	DOFBuffer *dofBuffer;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result;

	result = context->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result)) {
		return false;
	}

	matrixBuffer = (MatrixBuffer*)mappedResource.pData;

	matrixBuffer->world = world;
	matrixBuffer->view = view;
	matrixBuffer->projection = projection;

	context->Unmap(m_matrixBuffer, 0);

	result = context->Map(m_dofBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result)) {
		return false;
	}

	dofBuffer = (DOFBuffer*)mappedResource.pData;

	dofBuffer->unused = 0.0f;
	dofBuffer->range = (farZ - nearZ) / farZ * 0.04f;
	dofBuffer->nearZ = nearZ;
	dofBuffer->farZ = farZ;

	context->Unmap(m_dofBuffer, 0);

	result = context->Map(m_blurBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
		return false;

	blurBuffer = (BlurSamplersBuffer*)mappedResource.pData;

	float sigmaSquared = m_blurAmmount * m_blurAmmount;
	float offset = 0.0f;
	float totalWeights;

	blurBuffer->offsetAndWeight[0].x = 0.0f;
	totalWeights = blurBuffer->offsetAndWeight[0].y = GaussianFunction(sigmaSquared, offset);

	int count = BLUR_SAMPLE_COUNT / 2;

	for (int i = 0; i < count; i++) {
		offset = i + 1.0f;
		float weight = GaussianFunction(sigmaSquared, offset);
		blurBuffer->offsetAndWeight[i * 2 + 1].y = blurBuffer->offsetAndWeight[i * 2 + 2].y = weight;

		totalWeights += weight;

		float sampleOffset = i * 2.0f + 1.5f;

		blurBuffer->offsetAndWeight[i * 2 + 1].x = sampleOffset;
		blurBuffer->offsetAndWeight[i * 2 + 2].x = -sampleOffset;
	}
	for (int i = 0; i < BLUR_SAMPLE_COUNT; i++)
		blurBuffer->offsetAndWeight[i].y /= totalWeights;

	context->Unmap(m_blurBuffer, 0);

	m_blurVar->SetConstantBuffer(m_blurBuffer);
	m_WVPVar->SetConstantBuffer(m_matrixBuffer);
	m_screenSizeVar->SetConstantBuffer(m_screenSizeBuffer);
	m_dofVar->SetConstantBuffer(m_dofBuffer);

	return true;
}

void MMDEffect::RenderEffect(ID3D11DeviceContext *context, int indexCount)
{
}

Renderer::DXType<ID3D11Texture2D> MMDEffect::GetCurrentOutput()
{
	return m_currentBackTexture;
}

Renderer::DXType<ID3D11ShaderResourceView> MMDEffect::GetCurrentOutputView()
{
	return m_currentBackView;
}
