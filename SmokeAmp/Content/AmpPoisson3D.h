//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XSDXType.h"
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
	void ComputeDivergence(const AmpTexture3DView<U> &tvSource);
	void SolvePoisson(cfloat2 &vf, const uint8_t uIteration = 1);
	template<typename U>
	void Advect(cfloat fDeltaTime, const AmpTexture3DView<U> &tvSource);
	void SwapTextures(const bool bUnknown = false);

	const spAmpTexture3D<T>	&GetSrc() const { return m_pSrcKnown; }
	const spAmpTexture3D<T>	&GetDst() const { return m_pDstUnknown; }
	const spAmpTexture3D<T>	&GetTmp() const { return m_pSrcUnknown; }

protected:
	static float gaussSeidel(const AmpRWTexture3DView<float> &tvUnknownRW, const AmpTexture3DView<float> &tvKnownRO,
		cfloat2 &vf, const AmpIndex3D &idx) restrict(amp);
	void jacobi(cfloat2 &vf);

	spAmpTexture3D<T>	m_pSrcKnown;
	spAmpTexture3D<T>	m_pSrcUnknown;
	spAmpTexture3D<T>	m_pDstUnknown;

	float3				m_vSimSize;
};

#include "AmpPoisson3D.inl"
