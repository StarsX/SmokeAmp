//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "Common\amp_vector_math.h"

using AmpIndex1D = concurrency::index<1>;
using AmpIndex2D = concurrency::index<2>;
using AmpIndex3D = concurrency::index<3>;

template<typename T>
using AmpTexture1D = concurrency::graphics::texture<T, 1>;
template<typename T>
using AmpTexture1DView = concurrency::graphics::texture_view<const T, 1>;
template<typename T>
using AmpRWTexture1DView = concurrency::graphics::texture_view<T, 1>;
template<typename T>
using upAmpTexture1D = std::unique_ptr<AmpTexture1D<T>>;
template<typename T>
using spAmpTexture1D = std::shared_ptr<AmpTexture1D<T>>;

template<typename T>
using AmpTexture2D = concurrency::graphics::texture<T, 2>;
template<typename T>
using AmpTexture2DView = concurrency::graphics::texture_view<const T, 2>;
template<typename T>
using AmpRWTexture2DView = concurrency::graphics::texture_view<T, 2>;
template<typename T>
using upAmpTexture2D = std::unique_ptr<AmpTexture2D<T>>;
template<typename T>
using spAmpTexture2D = std::shared_ptr<AmpTexture2D<T>>;

template<typename T>
using AmpTexture3D = concurrency::graphics::texture<T, 3>;
template<typename T>
using AmpTexture3DView = concurrency::graphics::texture_view<const T, 3>;
template<typename T>
using AmpRWTexture3DView = concurrency::graphics::texture_view<T, 3>;
template<typename T>
using upAmpTexture3D = std::unique_ptr<AmpTexture3D<T>>;
template<typename T>
using spAmpTexture3D = std::shared_ptr<AmpTexture3D<T>>;

static inline float Gaussian3D(cfloat3 &vDisp, cfloat fRad) restrict(amp, cpu)
{
	const auto fRadSq = fRad * fRad;

	return concurrency::fast_math::exp(-4.0f * dot(vDisp, vDisp) / fRadSq);
}

static inline float3 Gradient3D(const AmpTexture3DView<float> &tvSource, const AmpIndex3D &idx) restrict(amp)
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

static inline float Divergence3D(const AmpTexture3DView<float4> &tvSource, const AmpIndex3D &idx) restrict(amp)
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
