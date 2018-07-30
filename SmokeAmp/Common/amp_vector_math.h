//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

using unorm = concurrency::graphics::unorm;
using unorm2 = concurrency::graphics::unorm_2;
using unorm3 = concurrency::graphics::unorm_3;
using unorm4 = concurrency::graphics::unorm_4;
using cunorm = const unorm;
using cunorm2 = const unorm2;
using cunorm3 = const unorm3;
using cunorm4 = const unorm4;

using float2 = concurrency::graphics::float_2;
using float3 = concurrency::graphics::float_3;
using float4 = concurrency::graphics::float_4;
using cfloat = const float;
using cfloat2 = const float2;
using cfloat3 = const float3;
using cfloat4 = const float4;

using uint = concurrency::graphics::uint;
using uint2 = concurrency::graphics::uint_2;
using uint3 = concurrency::graphics::uint_3;
using uint4 = concurrency::graphics::uint_4;
using cuint = const uint;
using cuint2 = const uint2;
using cuint3 = const uint3;
using cuint4 = const uint4;

using int2 = concurrency::graphics::int_2;
using int3 = concurrency::graphics::int_3;
using int4 = concurrency::graphics::int_4;
using cint = const int;
using cint2 = const int2;
using cint3 = const int3;
using cint4 = const int4;

struct float4x4
{
	float4 r[4];
};
using cfloat4x4 = const float4x4;

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
	return float2(concurrency::fast_math::floor(v.x), concurrency::fast_math::floor(v.y));
}

static inline float3 floor(cfloat3 &v) restrict(amp, cpu)
{
	return float3(
		concurrency::fast_math::floor(v.x),
		concurrency::fast_math::floor(v.y),
		concurrency::fast_math::floor(v.z)
		);
}

static inline float4 floor(cfloat4 &v) restrict(amp, cpu)
{
	return float4(
		concurrency::fast_math::floor(v.x),
		concurrency::fast_math::floor(v.y),
		concurrency::fast_math::floor(v.z),
		concurrency::fast_math::floor(v.z)
		);
}

template<typename T>
static inline float length(const T &v) restrict(amp, cpu)
{
	return concurrency::fast_math::sqrt(dot(v, v));
}

template<typename T>
static inline T normalize(const T &v) restrict(amp, cpu)
{
	return v * concurrency::fast_math::rsqrt(dot(v, v));
}

template<typename T>
static inline T lerp(const T &v1, const T &v2, const T &a) restrict(amp, cpu)
{
	return (1.0f - a) * v1 + a * v2;
}

template<typename T>
static inline T lerp(const T &v1, const T &v2, cfloat a) restrict(amp, cpu)
{
	return (1.0f - a) * v1 + a * v2;
}

static inline float4 mul(cfloat4x4 &m, cfloat4 &v) restrict(amp, cpu)
{
	return float4(dot(m.r[0], v), dot(m.r[1], v), dot(m.r[2], v), dot(m.r[3], v));
}

static inline float4 mul(cfloat4 &v, cfloat4x4 &m) restrict(amp, cpu)
{
	return v.x * m.r[0] + v.y * m.r[1] + v.z * m.r[2] + v.w * m.r[3];
}
