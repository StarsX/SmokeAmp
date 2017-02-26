//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

using float2 = concurrency::graphics::float_2;
using float3 = concurrency::graphics::float_3;
using float4 = concurrency::graphics::float_4;
using cfloat = const float;
using cfloat2 = const concurrency::graphics::float_2;
using cfloat3 = const concurrency::graphics::float_3;
using cfloat4 = const concurrency::graphics::float_4;

using uint2 = concurrency::graphics::uint_2;
using uint3 = concurrency::graphics::uint_3;
using uint4 = concurrency::graphics::uint_4;
using cuint = const concurrency::graphics::uint;
using cuint2 = const concurrency::graphics::uint_2;
using cuint3 = const concurrency::graphics::uint_3;
using cuint4 = const concurrency::graphics::uint_4;

using int2 = concurrency::graphics::int_2;
using int3 = concurrency::graphics::int_3;
using int4 = concurrency::graphics::int_4;
using cint = const int;
using cint2 = const concurrency::graphics::int_2;
using cint3 = const concurrency::graphics::int_3;
using cint4 = const concurrency::graphics::int_4;

static inline float dot(cfloat2 &v1, cfloat2 v2) restrict(amp, cpu)
{
	const auto vv = v1 * v2;
	return vv.x + vv.y;
}

static inline float dot(cfloat3 &v1, cfloat3 v2) restrict(amp, cpu)
{
	const auto vv = v1 * v2;
	return vv.x + vv.y + vv.z;
}

static inline float dot(cfloat4 &v1, cfloat4 v2) restrict(amp, cpu)
{
	const auto vv = v1 * v2;
	return vv.x + vv.y + vv.z + vv.w;
}

static inline float2 floor(cfloat2 &v) restrict(amp, cpu)
{
	return float2(concurrency::fast_math::floorf(v.x), concurrency::fast_math::floorf(v.y));
}

static inline float3 floor(cfloat3 &v) restrict(amp, cpu)
{
	return float3(
		concurrency::fast_math::floorf(v.x),
		concurrency::fast_math::floorf(v.y),
		concurrency::fast_math::floorf(v.z)
		);
}

static inline float4 floor(cfloat4 &v) restrict(amp, cpu)
{
	return float4(
		concurrency::fast_math::floorf(v.x),
		concurrency::fast_math::floorf(v.y),
		concurrency::fast_math::floorf(v.z),
		concurrency::fast_math::floorf(v.z)
		);
}

template<typename T>
static inline float length(T &v) restrict(amp, cpu)
{
	return concurrency::fast_math::sqrtf(dot(v, v));
}

template<typename T>
static inline T normalize(T &v) restrict(amp, cpu)
{
	return v * concurrency::fast_math::rsqrtf(dot(v, v));
}

template<typename T>
static inline T lerp(T v1, T v2, T a) restrict(amp, cpu)
{
	return (1.0f - a) * v1 + a * v2;
}
