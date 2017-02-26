//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "AmpFluid3D.h"

using namespace concurrency;
using namespace concurrency::direct3d;
using namespace concurrency::fast_math;
using namespace concurrency::graphics;
using namespace DX;
using namespace std;
using namespace ShaderIDs;
using namespace XSDX;

const auto g_pNullSRV = static_cast<LPDXShaderResourceView>(nullptr);	// Helper to Clear SRVs
const auto g_pNullUAV = static_cast<LPDXUnorderedAccessView>(nullptr);	// Helper to Clear UAVs
const auto g_iNullUint = 0u;											// Helper to Clear Buffers

AmpFluid3D::AmpFluid3D(const CPDXDevice &pDXDevice, const spShader &pShader, const spState &pState) :
	m_pDXDevice(pDXDevice),
	m_pShader(pShader),
	m_pState(pState),
	m_uSRField(0ui8),
	m_uSmpLinearClamp(1ui8),
	m_acclView(create_accelerator_view(pDXDevice.Get()))
{
	m_pDXDevice->GetImmediateContext(&m_pDXContext);
}

void AmpFluid3D::Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth)
{
	const auto fWidth = static_cast<float>(iWidth);
	const auto fHeight = static_cast<float>(iHeight);
	const auto fDepth = static_cast<float>(iDepth);
	m_vSimSize = float3(fWidth, fHeight, fDepth);

	// Create 3D textures
	m_pTxDensity = make_unique<AmpTexture<float>>(iDepth, iHeight, iWidth, m_acclView);
	m_pTxAdvDensity = make_unique<AmpTexture<float>>(iDepth, iHeight, iWidth, m_acclView);

	auto pTexture = CPDXTexture3D();
	auto pUnknown = graphics::direct3d::get_texture(dref(m_pTxDensity));
	pUnknown->QueryInterface<ID3D11Texture3D>(&pTexture);
	pUnknown->Release();

	const auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(pTexture.Get(), DXGI_FORMAT_R32_FLOAT, 0u, 1u);
	ThrowIfFailed(m_pDXDevice->CreateShaderResourceView(pTexture.Get(), &desc, &m_pSRVDensity));

	m_diffuse.Init(iWidth, iHeight, iDepth, m_acclView);
	m_pressure.Init(iWidth, iHeight, iDepth, m_acclView);
	m_pTxVelocity = m_diffuse.GetResult();
	m_pTxAdvVelocity = m_diffuse.GetKnown();
}

void AmpFluid3D::Simulate(cfloat fDeltaTime, cfloat4 vForceDens, cfloat3 vImLoc, const uint8_t uItVisc)
{
	advect(fDeltaTime);
	diffuse(uItVisc);
	impulse(fDeltaTime, vForceDens, vImLoc);
	project(fDeltaTime);
}

void AmpFluid3D::Render()
{
	m_pDXContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//m_pDXContext->IASetVertexBuffers(0u, 1u, &g_pNullBuffer, &g_iNullUint, &g_iNullUint);

	m_pDXContext->VSSetShader(m_pShader->GetVertexShader(g_uVSRayCast).Get(), nullptr, 0u);
	m_pDXContext->GSSetShader(nullptr, nullptr, 0u);
	m_pDXContext->PSSetShader(m_pShader->GetPixelShader(g_uPSRayCast).Get(), nullptr, 0u);
	
	m_pDXContext->PSSetShaderResources(m_uSRField, 1u, m_pSRVDensity.GetAddressOf());
	m_pDXContext->PSSetSamplers(m_uSmpLinearClamp, 1u, m_pState->LinearClamp().GetAddressOf());

	m_pDXContext->OMSetBlendState(m_pState->NonPremultiplied().Get(), nullptr, D3D11_DEFAULT_SAMPLE_MASK);

	m_pDXContext->Draw(36u, 0u);

	m_pDXContext->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);

	m_pDXContext->PSSetShaderResources(m_uSRField, 1u, &g_pNullSRV);

	m_pDXContext->VSSetShader(nullptr, nullptr, 0);
	m_pDXContext->PSSetShader(nullptr, nullptr, 0);
	m_pDXContext->RSSetState(0);
}

void AmpFluid3D::advect(cfloat fDeltaTime)
{
	static const auto fDissipation = 0.996f;

	const auto tvVelocityRW = AmpRWTextureView<float4>(dref(m_pTxAdvVelocity));
	const auto tvDensityRW = AmpRWTextureView<float>(dref(m_pTxAdvDensity));
	const auto tvVelocityRO = AmpTextureView<float4>(dref(m_pTxVelocity));
	const auto tvDensityRO = AmpTextureView<float>(dref(m_pTxDensity));

	const auto vTexel = 1.0f / m_vSimSize;

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvVelocityRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
	{
		const auto vLoc = float3((float)idx[2], (float)idx[1], (float)idx[0]);
		
		// Velocity tracing
		const auto vU = tvVelocityRO[idx].xyz;
		const auto vTex = (vLoc + 0.5f) * vTexel - vU;

		// Update velocity and density
		tvVelocityRW.set(idx, tvVelocityRO.sample<filter_linear, address_clamp>(vTex));
		tvDensityRW.set(idx, tvDensityRO.sample<filter_linear, address_clamp>(vTex) * fDissipation);
	}
	);
}

void AmpFluid3D::diffuse(const uint8_t uIteration)
{
	if (uIteration > 0u) m_diffuse.SolvePoisson(uIteration);
	else
	{
		m_diffuse.SwapTextures();
		m_pTxAdvVelocity = m_diffuse.GetKnown();
	}
	m_pTxVelocity = m_diffuse.GetResult();
}

void AmpFluid3D::impulse(cfloat fDeltaTime, cfloat4 &vForceDens, cfloat3 &vImLoc)
{
	const auto tvVelocityRW = AmpRWTextureView<float4>(dref(m_pTxAdvVelocity));
	const auto tvDensityRW = AmpRWTextureView<float>(dref(m_pTxDensity));
	const auto tvVelocityRO = AmpTextureView<float4>(dref(m_pTxVelocity));
	const auto tvDensityRO = AmpTextureView<float>(dref(m_pTxAdvDensity));

	const auto vTexel = 1.0f / m_vSimSize;

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvVelocityRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
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
}

void AmpFluid3D::project(cfloat fDeltaTime)
{
	{
		const auto tvVelocityRO = AmpTextureView<float4>(dref(m_pTxAdvVelocity));
		m_pressure.ComputeDivergence(tvVelocityRO);
		m_pressure.SolvePoisson(cfloat2(-1.0f, 6.0f));
	}

	bound();

	// Projection
	{
		auto txPressure = m_pressure.GetResult();
		const auto tvVelocityRW = AmpRWTextureView<float4>(dref(m_pTxAdvVelocity));
		const auto tvVelocityRO = AmpTextureView<float4>(dref(m_pTxVelocity));
		const auto tvPressureRO = AmpTextureView<float>(dref(txPressure));

		parallel_for_each(
			// Define the compute domain, which is the set of threads that are created.
			tvVelocityRW.extent,
			// Define the code to run on each thread on the accelerator.
			[=](const AmpIndex idx) restrict(amp)
		{
			// Project the velocity onto its divergence-free component
			const auto vVelocity = tvVelocityRO[idx].xyz - Gradient3D(tvPressureRO, idx) / REST_DENS;
			tvVelocityRW.set(idx, float4(vVelocity.x, vVelocity.y, vVelocity.z, 0.0f));
		}
		);
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
	const auto tvVelocityRW = AmpRWTextureView<float4>(dref(m_pTxVelocity));
	const auto tvVelocityRO = AmpTextureView<float4>(dref(m_pTxAdvVelocity));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvVelocityRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
	{
		// Current location
		const auto vMax = int3(tvVelocityRW.extent[2], tvVelocityRW.extent[1], tvVelocityRW.extent[0]) - 1;
		auto vLoc = idx;

		const int3 vOffset = {
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
}
