//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "Common\amp_vector_math.h"

using AmpIndex = concurrency::index<g_iDim>;

template<typename T>
using AmpTexture = concurrency::graphics::texture<T, g_iDim>;
template<typename T>
using AmpTextureView = concurrency::graphics::texture_view<const T, g_iDim>;
template<typename T>
using AmpRWTextureView = concurrency::graphics::texture_view<T, g_iDim>;
template<typename T>
using spAmpTexture = std::shared_ptr<AmpTexture<T>>;

static inline float Gaussian3D(cfloat3 &vDisp, cfloat fRad) restrict(amp, cpu)
{
	const auto fRadSq = fRad * fRad;

	return concurrency::fast_math::exp(-4.0f * dot(vDisp, vDisp) / fRadSq);
}

static inline float3 Gradient3D(const AmpTextureView<float> &tvSource, const AmpIndex &idx) restrict(amp)
{
	// Get values from neighboring cells
	const auto fxL = tvSource(idx[0], idx[1], idx[2] - 1);
	const auto fxR = tvSource(idx[0], idx[1], idx[2] + 1);
	const auto fyU = tvSource(idx[0], idx[1] - 1, idx[2]);
	const auto fyD = tvSource(idx[0], idx[1] + 1, idx[2]);
	const auto fzF = tvSource(idx[0] - 1, idx[1], idx[2]);
	const auto fzB = tvSource(idx[0] + 1, idx[1], idx[2]);

	// Compute the velocity's divergence using central differences
	return 0.5 * float3(fxR - fxL, fyD - fyU, fzB - fzF);
}

static inline float Divergence3D(const AmpTextureView<float4> &tvSource, const AmpIndex &idx) restrict(amp)
{
	// Get values from neighboring cells
	const auto fxL = tvSource(idx[0], idx[1], idx[2] - 1).x;
	const auto fxR = tvSource(idx[0], idx[1], idx[2] + 1).x;
	const auto fyU = tvSource(idx[0], idx[1] - 1, idx[2]).y;
	const auto fyD = tvSource(idx[0], idx[1] + 1, idx[2]).y;
	const auto fzF = tvSource(idx[0] - 1, idx[1], idx[2]).z;
	const auto fzB = tvSource(idx[0] + 1, idx[1], idx[2]).z;

	// Take central differences of neighboring values
	return 0.5f * (fxR - fxL + fyD - fyU + fzB - fzF);
}
