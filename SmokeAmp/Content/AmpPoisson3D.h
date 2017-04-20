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

	void Init(cuint3 &vSimSize, const uint8_t bitWidth, AmpAcclView &acclView);
	void Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth,
		const uint8_t bitWidth, AmpAcclView &acclView);
	template<typename U>
	void ComputeDivergence(const AmpTextureView<U> &tvSource);
	void SolvePoisson(cfloat2 &vf, const uint8_t uIteration = 1ui8);
	template<typename U>
	void Advect(cfloat fDeltaTime, const AmpTextureView<U> &tvSource);
	void SwapTextures(const bool bUnknown = false);

	const spAmpTexture<T>	&GetSrc() const { return m_pSrcKnown; }
	const spAmpTexture<T>	&GetDst() const { return m_pDstUnknown; }
	const spAmpTexture<T>	&GetTmp() const { return m_pSrcUnknown; }

protected:
	static float gaussSeidel(const AmpRWTextureView<float> &tvUnknownRW, const AmpTextureView<float> &tvKnownRO,
		cfloat2 &vf, const AmpIndex &idx) restrict(amp);
	void jacobi(cfloat2 &vf);

	spAmpTexture<T>		m_pSrcKnown;
	spAmpTexture<T>		m_pSrcUnknown;
	spAmpTexture<T>		m_pDstUnknown;

	float3				m_vSimSize;
};

#include "AmpPoisson3D.inl"
