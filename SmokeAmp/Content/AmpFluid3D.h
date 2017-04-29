//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "AmpPoisson3D.h"

#define VISC_ITERATION	0

class AmpFluid3D
{
public:
	struct CBImmutable
	{
		float4	m_vDirectional;
		float4	m_vAmbient;
	};

	struct CBPerObject
	{
		float4		m_vLocalSpaceLightPt;
		float4		m_vLocalSpaceEyePt;
		float4x4	m_mScreenToLocal;
	};

	AmpFluid3D(const AmpAcclView &acclView);

	void Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth);
	void Simulate(
		cfloat fDeltaTime,
		const AmpTexture3DView<float4> &tvImpulseRO,
		const uint8_t uItVisc = VISC_ITERATION
		);
	void Simulate(
		cfloat fDeltaTime,
		cfloat4 vForceDens = float4(0.0f, 0.0f, 0.0f, 0.0f),
		cfloat3 vImLoc = float3(0.0f, 0.0f, 0.0f),
		const uint8_t uItVisc = VISC_ITERATION
		);
	void Render(upAmpTexture2D<unorm4> &pDst, const AmpTexture3DView<float> &tvDepthRO,
		const CBImmutable &cbImmutable, const CBPerObject &cbPerObj);
	void Render(upAmpTexture2D<unorm4> &pDst, const CBImmutable &cbImmutable,
		const CBPerObject &cbPerObj);

	const AmpAcclView &GetAcceleratorView() const { return m_acclView; }

protected:
	void advect(cfloat fDeltaTime);
	void advect(cfloat fDeltaTime, const AmpTexture3DView<float4> &tvVelocityRO);
	void diffuse(const uint8_t uIteration);
	void impulse(cfloat fDeltaTime, cfloat4 &vForceDens, cfloat3 &vImLoc);
	void project(cfloat fDeltaTime);
	void bound();

	spAmpTexture3D<float4>			m_pSrcVelocity;
	spAmpTexture3D<float4>			m_pDstVelocity;
	spAmpTexture3D<float>			m_pSrcDensity;
	spAmpTexture3D<float>			m_pDstDensity;
	spAmpTexture3D<float>			m_pTmpDensity;

	float3							m_vSimSize;

	AmpPoisson3D<float4>			m_diffuse;
	AmpPoisson3D<float>				m_pressure;

	AmpAcclView						m_acclView;
};

using upAmpFluid3D = std::unique_ptr<AmpFluid3D>;
using spAmpFluid3D = std::shared_ptr<AmpFluid3D>;
using vuAmpFluid3D = std::vector<upAmpFluid3D>;
using vpAmpFluid3D = std::vector<spAmpFluid3D>;
