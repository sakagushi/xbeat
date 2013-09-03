#pragma once

#include "../DXUtil.h"
#include "../D3DRenderer.h"
#include "../D3DTextureRenderer.h"
#include "../OrthoWindowClass.h"
#include <memory>
#include <d3dx11effect.h>
#include <string>

namespace Renderer {
namespace Shaders {

class MMDEffect
{
public:
	struct MatrixBuffer {
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX projection;
	};

	struct ScreenSizeBuffer {
		DirectX::XMFLOAT2 dimentions;
		DirectX::XMFLOAT2 texelSize;
	};

	__declspec(align(16)) struct DOFBuffer {
		float nearZ;
		float farZ;
		float range;
		float unused;
	};

	MMDEffect(void);
	~MMDEffect(void);

	bool Initialize(ID3D11Device *device, HWND wnd, int width, int height);
	void Shutdown();
	bool Render(std::shared_ptr<D3DRenderer> d3d, int indexCount, DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, std::shared_ptr<D3DTextureRenderer> renderTexture, std::shared_ptr<OrthoWindowClass> window, float farZ, float nearZ);
	DXType<ID3D11Texture2D> GetCurrentOutput();
	DXType<ID3D11ShaderResourceView> GetCurrentOutputView();

	int bufferIndex;
private:
	bool InitializeEffect(ID3D11Device *device, HWND wnd, int width, int height, const std::wstring &filename);
	void ShutdownEffect();
	void OutputErrorMessage(ID3D10Blob *error, HWND wnd, const std::wstring &filename);

	bool SetEffectParameters(ID3D11DeviceContext *context, DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, ID3D11ShaderResourceView **textures, int textureCount, float farZ, float nearZ);
	void RenderEffect(ID3D11DeviceContext *context, int indexCount);

	DXType<ID3DX11Effect> m_effect;
	DXType<ID3D11SamplerState> m_sampler;
	DXType<ID3D11InputLayout> m_layout;
	DXType<ID3D11Buffer> m_matrixBuffer;
	DXType<ID3D11Buffer> m_screenSizeBuffer;
	DXType<ID3D11Buffer> m_dofBuffer;
	DXType<ID3D11Texture2D> m_originalBackTexture;
	DXType<ID3D11ShaderResourceView> m_originalBackView;
	DXType<ID3D11Texture2D> m_currentBackTexture;
	DXType<ID3D11ShaderResourceView> m_currentBackView;
	DXType<ID3D11Texture2D> m_originalDepthTexture;
	DXType<ID3D11ShaderResourceView> m_originalDepthView;

	uint32_t m_numGroups;
	uint32_t *m_numTechniques;
	uint32_t **m_numPasses;
};

}
}
