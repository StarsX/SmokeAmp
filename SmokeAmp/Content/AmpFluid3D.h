//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "AmpPoisson3D.h"

#define VISC_ITERATION	0

class AmpFluid3D
{
public:
	AmpFluid3D(const XSDX::CPDXDevice &pDXDevice, const XSDX::spShader &pShader, const XSDX::spState &pState);

	void Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth);
	void Simulate(
		cfloat fDeltaTime,
		const XSDX::CPDXShaderResourceView &pImpulseSRV,
		const uint8_t uItVisc = VISC_ITERATION
		);
	void Simulate(
		cfloat fDeltaTime,
		cfloat4 vForceDens = float4(0.0f, 0.0f, 0.0f, 0.0f),
		cfloat3 vImLoc = float3(0.0f, 0.0f, 0.0f),
		const uint8_t uItVisc = VISC_ITERATION
		);
	void Render(const XSDX::CPDXShaderResourceView &pDepthSRV);
	void Render();

protected:
	void advect(cfloat fDeltaTime);
	void diffuse(const uint8_t uIteration);
	void impulse(cfloat fDeltaTime, cfloat4 &vForceDens, cfloat3 &vImLoc);
	void project(cfloat fDeltaTime);
	void bound();

	spAmpTexture<float4>			m_pTxVelocity;
	spAmpTexture<float4>			m_pTxAdvVelocity;
	spAmpTexture<float>				m_pTxDensity;
	spAmpTexture<float>				m_pTxAdvDensity;

	XSDX::CPDXShaderResourceView	m_pSRVDensity;

	uint8_t							m_uSRField;
	uint8_t							m_uSmpLinearClamp;

	float3							m_vSimSize;

	AmpPoisson3D<float4>			m_diffuse;
	AmpPoisson3D<float>				m_pressure;

	XSDX::spShader					m_pShader;
	XSDX::spState					m_pState;

	XSDX::CPDXDevice				m_pDXDevice;
	XSDX::CPDXContext				m_pDXContext;

	AmpAcclView						m_acclView;
};

using upAmpFluid3D = std::unique_ptr<AmpFluid3D>;
using spAmpFluid3D = std::shared_ptr<AmpFluid3D>;
using vuAmpFluid3D = std::vector<upAmpFluid3D>;
using vpAmpFluid3D = std::vector<spAmpFluid3D>;
