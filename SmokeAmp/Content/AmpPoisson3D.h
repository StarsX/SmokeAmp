//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XSDXState.h"
#include "XSDXShader.h"
#include "XSDXShaderCommon.h"
#include "ShaderCommon3D.h"

const int	g_iDim = 3;
#include "FieldMath.h"

using AmpAcclView = concurrency::accelerator_view;

template<typename T>
class AmpPoisson3D
{
public:
	AmpPoisson3D();

	void Init(const DirectX::XMUINT3 &vSimSize, AmpAcclView &acclView);
	void Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth, AmpAcclView &acclView);
	template<typename U>
	void ComputeDivergence(const AmpTextureView<U> &tvSource);
	void SolvePoisson(cfloat2 &vf, const uint8_t uIteration = 1ui8);
	template<typename U>
	void Advect(cfloat fDeltaTime, const AmpTextureView<U> &tvSource);
	void SwapTextures();

	const spAmpTexture<T>	&GetKnown()		const { return m_pTxKnown; }
	const spAmpTexture<T>	&GetResult()	const { return m_pTxUnknown; }

protected:
	static float gaussSeidel(const AmpRWTextureView<float> &tvUnknownRW, const AmpTextureView<float> &tvKnownRO,
		cfloat2 &vf, const AmpIndex &idx) restrict(amp);
	void jacobi(cfloat2 &vf);

	spAmpTexture<T>		m_pTxKnown;
	spAmpTexture<T>		m_pTxUnknown;
	spAmpTexture<T>		m_pTxPingpong;

	float3				m_vSimSize;
};

#include "AmpPoisson3D.inl"
