//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "AmpFluid3D.h"

#define NUM_SAMPLES			128
#define NUM_LIGHT_SAMPLES	32
#define ABSORPTION			1.0f
#define ZERO_THRESHOLD		0.01f
//#define ONE_THRESHOLD		0.999f

using namespace concurrency;
using namespace concurrency::direct3d;
using namespace concurrency::fast_math;
using namespace concurrency::graphics;
using namespace std;
using namespace XSDX;

// Screen space to loacal space
inline float3 ScreenToLocal(cfloat3 &vLoc, cfloat4x4 &mScreenToLocal) restrict(amp)
{
	float4 vPos = mul(mScreenToLocal, float4(vLoc.x, vLoc.y, vLoc.z, 1.0f));

	return vPos.xyz / vPos.w;
}

// Compute start point of the ray
inline bool ComputeStartPoint(float3 &vPos, cfloat3 vRayDir) restrict(amp)
{
	if (fabs(vPos.x) <= 1.0 && fabs(vPos.y) <= 1.0 && fabs(vPos.z) <= 1.0) return true;

	cfloat aPos[3] = { vPos.x, vPos.y, vPos.z };
	cfloat aRayDir[3] = { vRayDir.x, vRayDir.y, vRayDir.z };

	//float U = asfloat(0x7f800000);	// INF
	auto U = FLT_MAX;
	auto bHit = false;

	for (uint i = 0; i < 3; ++i)
	{
		const auto u = ((aRayDir[i] < 0.0f ? 1.0f : -1.0f) - aPos[i]) / aRayDir[i];
		if (u < 0.0f) continue;

		const auto j = (i + 1) % 3, k = (i + 2) % 3;
		if (fabs(aRayDir[j] * u + aPos[j]) > 1.0f) continue;
		if (fabs(aRayDir[k] * u + aPos[k]) > 1.0f) continue;
		if (u < U)
		{
			U = u;
			bHit = true;
		}
	}

	vPos += vRayDir * U;
	vPos.x = clamp(vPos.x, -1.0f, 1.0f);
	vPos.y = clamp(vPos.y, -1.0f, 1.0f);
	vPos.z = clamp(vPos.z, -1.0f, 1.0f);

	return bHit;
}

AmpFluid3D::AmpFluid3D(const AmpAcclView &acclView) :
	m_acclView(acclView)
{
}

void AmpFluid3D::Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth)
{
	const auto fWidth = static_cast<float>(iWidth);
	const auto fHeight = static_cast<float>(iHeight);
	const auto fDepth = static_cast<float>(iDepth);
	m_vSimSize = float3(fWidth, fHeight, fDepth);

	// Create 3D textures
	m_pSrcDensity = make_unique<AmpTexture3D<float>>(iDepth, iHeight, iWidth, 16u, m_acclView);
	m_pDstDensity = make_unique<AmpTexture3D<float>>(iDepth, iHeight, iWidth, 16u, m_acclView);
#ifdef _MACCORMACK_
	m_pTmpDensity = make_unique<AmpTexture<float>>(iDepth, iHeight, iWidth, 16u, m_acclView);
#endif

	m_diffuse.Init(iWidth, iHeight, iDepth, 16ui8, m_acclView);
	m_pressure.Init(iWidth, iHeight, iDepth, 32ui8, m_acclView);
	m_pSrcVelocity = m_diffuse.GetSrc();
	m_pDstVelocity = m_diffuse.GetDst();
}

void AmpFluid3D::Simulate(cfloat fDeltaTime, cfloat4 vForceDens, cfloat3 vImLoc, const uint8_t uItVisc)
{
	advect(fDeltaTime);
	diffuse(uItVisc);
	impulse(fDeltaTime, vForceDens, vImLoc);
	project(fDeltaTime);
}

void AmpFluid3D::Render(upAmpTexture2D<unorm4> &pDst, const CBImmutable &cbImmutable, const CBPerObject &cbPerObj)
{
	const auto tvDstRW = AmpRWTexture2DView<unorm4>(dref(pDst));
	const auto tvDensityRO = AmpTexture3DView<float>(dref(m_pSrcDensity));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvDstRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex2D idx) restrict(amp)
	{
		const auto vCornflowerBlue = float3(0.392156899f, 0.584313750f, 0.929411829f);
		const auto vClear = vCornflowerBlue * vCornflowerBlue;

		const auto fMaxDist = 2.0f * sqrt(3.0f);
		const auto fStepScale = fMaxDist / NUM_SAMPLES;
		const auto fLStepScale = fMaxDist / NUM_LIGHT_SAMPLES;

		// Constant buffer immutable
		const auto vLightRad = cbImmutable.m_vDirectional.xyz * cbImmutable.m_vDirectional.w;
		const auto vAmbientRad = cbImmutable.m_vAmbient.xyz * cbImmutable.m_vAmbient.w;

		// Constant buffer per object
		const auto &vLocalSpaceLightPt = cbPerObj.m_vLocalSpaceLightPt.xyz;
		const auto &vLocalSpaceEyePt = cbPerObj.m_vLocalSpaceEyePt.xyz;
		const auto &mScreenToLocal = cbPerObj.m_mScreenToLocal;

		//////////////////////////////////////////////////////////////////////////////////////////

		const auto vLoc = float3((float)idx[1], (float)idx[0], 0.0f);

		auto vPos = ScreenToLocal(vLoc, mScreenToLocal);			// The point on the near plane
		const auto vRayDir = normalize(vPos - vLocalSpaceEyePt);
		if (!ComputeStartPoint(vPos, vRayDir)) return;

		const auto vStep = vRayDir * fStepScale;

#ifndef _POINT_LIGHT_
		const auto vLRStep = normalize(vLocalSpaceLightPt) * fLStepScale;
#endif

		// Transmittance
		float fTransmit = 1.0f;
		// In-scattered radiance
		float fScatter = 0.0f;

		for (uint i = 0; i < NUM_SAMPLES; ++i)
		{
			if (fabs(vPos.x) > 1.0f || fabs(vPos.y) > 1.0f || fabs(vPos.z) > 1.0f) break;
			auto vTex = float3(0.5f, -0.5f, 0.5f) * vPos + 0.5f;

			// Get a sample
			const auto fDens = fmin(tvDensityRO.sample(vTex), 16.0f);

			// Skip empty space
			if (fDens > ZERO_THRESHOLD)
			{
				// Attenuate ray-throughput
				const auto fScaledDens = fDens * fStepScale;
				fTransmit *= saturate(1.0f - fScaledDens * ABSORPTION);
				if (fTransmit < ZERO_THRESHOLD) break;

				// Point light direction in texture space
#ifdef _POINT_LIGHT_
				const auto vLRStep = normalize(vLocalSpaceLightPt - vPos) * fLStepScale;
#endif

				// Sample light
				auto fLRTrans = 1.0f;	// Transmittance along light ray
				auto vLRPos = vPos + vLRStep;

				for (uint j = 0; j < NUM_LIGHT_SAMPLES; ++j)
				{
					if (fabs(vLRPos.x) > 1.0f || fabs(vLRPos.y) > 1.0f || fabs(vLRPos.z) > 1.0f) break;
					vTex = float3(0.5f, -0.5f, 0.5f) * vLRPos + 0.5f;

					// Get a sample along light ray
					cfloat fLRDens = fmin(tvDensityRO.sample(vTex), 16.0f);

					// Attenuate ray-throughput along light direction
					fLRTrans *= saturate(1.0f - ABSORPTION * fLStepScale * fLRDens);
					if (fLRTrans < ZERO_THRESHOLD) break;

					// Update position along light ray
					vLRPos += vLRStep;
				}

				fScatter += fLRTrans * fTransmit * fScaledDens;
			}

			vPos += vStep;
		}

		//clip(ONE_THRESHOLD - fTransmit);

		auto vResult = fScatter * vLightRad + vAmbientRad;
		vResult = lerp(vResult, vClear, fTransmit);

		tvDstRW.set(idx, unorm4(sqrt(vResult.x), sqrt(vResult.y), sqrt(vResult.z), 1.0f));
	}
	);
}

void AmpFluid3D::advect(cfloat fDeltaTime)
{
	auto tvVelocityRO = AmpTexture3DView<float4>(dref(m_pSrcVelocity));
	advect(fDeltaTime, tvVelocityRO);

#ifdef _MACCORMACK_
	m_pDiffuse->SwapTextures(true);
	m_pDstVelocity = m_pDiffuse->GetDst();
	pSrcVelocity = m_pDiffuse->GetTmp()->GetSRV();
	m_pTmpDensity.swap(m_pDstDensity);
	advect(-fDeltaTime, pSrcVelocity);

	macCormack(fDeltaTime, pSrcVelocity);
#endif
}

void AmpFluid3D::advect(cfloat fDeltaTime, const AmpTexture3DView<float4> &tvVelocityRO)
{
#ifdef _MACCORMACK_
	static const auto fDecay = 1.0f;
#else
	static const auto fDecay = 0.996f;
#endif

	const auto tvPhiVelRW = AmpRWTexture3DView<float4>(dref(m_pDstVelocity));
	const auto tvPhiDenRW = AmpRWTexture3DView<float>(dref(m_pDstDensity));
	const auto tvPhiVelRO = AmpTexture3DView<float4>(dref(m_pSrcVelocity));
	const auto tvPhiDenRO = AmpTexture3DView<float>(dref(m_pSrcDensity));

	const auto vTexel = 1.0f / m_vSimSize;

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvPhiVelRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex3D idx) restrict(amp)
	{
		const auto vLoc = float3((float)idx[2], (float)idx[1], (float)idx[0]);
		
		// Velocity tracing
		const auto vU = tvVelocityRO[idx].xyz;
		const auto vTex = (vLoc + 0.5f) * vTexel - vU * fDeltaTime;

		// Update velocity and density
		tvPhiVelRW.set(idx, tvPhiVelRO.sample(vTex));
		tvPhiDenRW.set(idx, tvPhiDenRO.sample(vTex) * fDecay);
	}
	);

	// Swap buffers
	m_diffuse.SwapTextures();
	m_pSrcVelocity = m_diffuse.GetSrc();
	m_pDstVelocity = m_diffuse.GetDst();
	m_pSrcDensity.swap(m_pDstDensity);
}

void AmpFluid3D::diffuse(const uint8_t uIteration)
{
	if (uIteration > 0u)
	{
		m_diffuse.SolvePoisson(uIteration);
		m_pSrcVelocity = m_diffuse.GetSrc();
		m_pDstVelocity = m_diffuse.GetDst();
	}
}

void AmpFluid3D::impulse(cfloat fDeltaTime, cfloat4 &vForceDens, cfloat3 &vImLoc)
{
	const auto tvVelocityRW = AmpRWTexture3DView<float4>(dref(m_pDstVelocity));
	const auto tvDensityRW = AmpRWTexture3DView<float>(dref(m_pDstDensity));
	const auto tvVelocityRO = AmpTexture3DView<float4>(dref(m_pSrcVelocity));
	const auto tvDensityRO = AmpTexture3DView<float>(dref(m_pSrcDensity));

	const auto vTexel = 1.0f / m_vSimSize;

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvVelocityRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex3D idx) restrict(amp)
	{
		const auto vLoc = float3((float)idx[2], (float)idx[1], (float)idx[0]);
		const auto vTex = vLoc * vTexel;
		const auto fBasis = Gaussian3D(vTex - vImLoc, 0.032f);

		const auto fDens = length(vForceDens.xyz) * vForceDens.w;
		const auto vForce = vForceDens.xyz * fBasis;

		const auto vVelocity = tvVelocityRO[idx].xyz + vForce * fDeltaTime;

		tvVelocityRW.set(idx, float4(vVelocity.x, vVelocity.y, vVelocity.z, 0.0f));
		tvDensityRW.set(idx, tvDensityRO[idx] + fDens * fBasis);
	}
	);

	// Swap buffers
	m_diffuse.SwapTextures();
	m_pSrcVelocity = m_diffuse.GetSrc();
	m_pDstVelocity = m_diffuse.GetDst();
	m_pSrcDensity.swap(m_pDstDensity);
}

void AmpFluid3D::project(cfloat fDeltaTime)
{
	{
		const auto tvVelocityRO = AmpTexture3DView<float4>(dref(m_pSrcVelocity));
		m_pressure.ComputeDivergence(tvVelocityRO);
		m_pressure.SolvePoisson(cfloat2(-1.0f, 6.0f));
	}

	bound();

	// Projection
	{
		auto txPressure = m_pressure.GetSrc();
		const auto tvVelocityRW = AmpRWTexture3DView<float4>(dref(m_pDstVelocity));
		const auto tvVelocityRO = AmpTexture3DView<float4>(dref(m_pSrcVelocity));
		const auto tvPressureRO = AmpTexture3DView<float>(dref(txPressure));

		parallel_for_each(
			// Define the compute domain, which is the set of threads that are created.
			tvVelocityRW.extent,
			// Define the code to run on each thread on the accelerator.
			[=](const AmpIndex3D idx) restrict(amp)
		{
			// Project the velocity onto its divergence-free component
			const auto vVelocity = tvVelocityRO[idx].xyz - Gradient3D(tvPressureRO, idx) / REST_DENS;
			tvVelocityRW.set(idx, float4(vVelocity.x, vVelocity.y, vVelocity.z, 0.0f));
		}
		);

		// Swap buffers
		m_diffuse.SwapTextures();
		m_pSrcVelocity = m_diffuse.GetSrc();
		m_pDstVelocity = m_diffuse.GetDst();
	}

	bound();

#ifdef _ADVECT_PRESSURE_
	// Temporal optimization
	{
		const auto tvVelocityRO = AmpTextureView<float4>(dref(m_pTxVelocity));
		m_pressure.Advect(fDeltaTime, tvVelocityRO);
	}
#endif
}

void AmpFluid3D::bound()
{
	const auto tvVelocityRW = AmpRWTexture3DView<float4>(dref(m_pDstVelocity));
	const auto tvVelocityRO = AmpTexture3DView<float4>(dref(m_pSrcVelocity));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvVelocityRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex3D idx) restrict(amp)
	{
		// Current location
		const auto vMax = int3(tvVelocityRW.extent[2], tvVelocityRW.extent[1], tvVelocityRW.extent[0]) - 1;
		auto vLoc = idx;

		const int3 vOffset =
		{
			vLoc[2] >= vMax.x ? -1 : (vLoc[2] <= 0 ? 1 : 0),
			vLoc[1] >= vMax.y ? -1 : (vLoc[1] <= 0 ? 1 : 0),
			vLoc[0] >= vMax.z ? -1 : (vLoc[0] <= 0 ? 1 : 0)
		};
		vLoc[0] += vOffset.z;
		vLoc[1] += vOffset.y;
		vLoc[2] += vOffset.x;

		if (vOffset.x || vOffset.y || vOffset.z)
			tvVelocityRW.set(idx, -tvVelocityRO[vLoc]);
		else tvVelocityRW.set(idx, tvVelocityRO[idx]);
	}
	);

	// Swap buffers
	m_diffuse.SwapTextures();
	m_pSrcVelocity = m_diffuse.GetSrc();
	m_pDstVelocity = m_diffuse.GetDst();
}
