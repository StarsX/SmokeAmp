//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#define THREAD_BLOCK_X		8
#define THREAD_BLOCK_Y		8
#define THREAD_BLOCK_Z		8

#define REST_DENS			0.56

template<typename T>
inline AmpPoisson3D<T>::AmpPoisson3D()
{
}

template<typename T>
inline void AmpPoisson3D<T>::Init(const DirectX::XMUINT3 &vSimSize, AmpAcclView &acclView)
{
	Init(vSimSize.x, vSimSize.y, vSimSize.z, acclView);
}

template<typename T>
inline void AmpPoisson3D<T>::Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth, AmpAcclView &acclView)
{
	const auto fWidth = static_cast<float>(iWidth);
	const auto fHeight = static_cast<float>(iHeight);
	const auto fDepth = static_cast<float>(iDepth);
	m_vSimSize = float3(fWidth, fHeight, fDepth);
	
	// Initialize data
	//const auto uByteWidth = sizeof(T) * uWidth * uHeight * uDepth;
	//auto vData = vbyte(uByteWidth);
	//ZeroMemory(vData.data(), uByteWidth);

	// Create 3D textures
	m_pTxKnown = make_shared<AmpTexture<T>>(iDepth, iHeight, iWidth, acclView);
	m_pTxUnknown = make_shared<AmpTexture<T>>(iDepth, iHeight, iWidth, acclView);
	m_pTxPingpong = make_shared<AmpTexture<T>>(iDepth, iHeight, iWidth, acclView);
}

template<typename T>
template<typename U>
inline void AmpPoisson3D<T>::ComputeDivergence(const AmpTextureView<U> &tvSource)
{
	const auto tvKnownRW = AmpRWTextureView<T>(dref(m_pTxKnown));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvKnownRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
	{
		tvKnownRW.set(idx, Divergence3D(tvSource, idx));
	}
	);
}

template<>
inline void AmpPoisson3D<float>::SolvePoisson(cfloat2 &vf, const uint8_t uIteration)
{
	const auto tvUnknownRW = AmpRWTextureView<float>(*m_pTxUnknown);
	const auto tvKnownRO = AmpTextureView<float>(*m_pTxKnown);

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvUnknownRW.extent.tile<THREAD_BLOCK_X, THREAD_BLOCK_Y, THREAD_BLOCK_Z>(),
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
	{
		// Unordered Gauss-Seidel iteration
		for (concurrency::graphics::uint i = 0; i < 1024; ++i)
		{
			const auto fPressPRev = tvUnknownRW[idx];
			const auto fPress = gaussSeidel(tvUnknownRW, tvKnownRO, vf, idx);

			if (concurrency::fast_math::fabsf(fPress - fPressPRev) < 0.0000001f) return;
			tvUnknownRW.set(idx, fPress);
		}
	}
	);
}

template<typename T>
inline void AmpPoisson3D<T>::SolvePoisson(cfloat2 &vf, const uint8_t uIteration)
{
	for (auto i = 0ui8; i < uIteration; ++i) jacobi(vf);
}

template<typename T>
template<typename U>
inline void AmpPoisson3D<T>::Advect(cfloat fDeltaTime, const AmpTextureView<U>& tvSource)
{
	const auto tvUnknownRW = AmpRWTextureView<T>(dref(m_pTxPingpong));
	const auto tvUnknownRO = AmpTextureView<T>(dref(m_pTxUnknown));

	const auto vTexel = 1.0f / m_vSimSize;

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvUnknownRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
	{
		const auto vLoc = float3((float)idx[2], (float)idx[1], (float)idx[0]);

		// Velocity tracing
		const auto vU = tvSource[idx].xyz;
		const auto vTex = (vLoc + 0.5f) * vTexel - vU;

		// Update
		tvUnknownRW.set(idx, tvUnknownRO.sample<filter_linear, address_clamp>(vTex));
	}
	);

	// Swap
	m_pTxPingpong.swap(m_pTxUnknown);
}

template<typename T>
inline void AmpPoisson3D<T>::SwapTextures()
{
	m_pTxKnown.swap(m_pTxUnknown);
}

template<typename T>
inline float AmpPoisson3D<T>::gaussSeidel(const AmpRWTextureView<float> &tvUnknownRW,
	const AmpTextureView<float> &tvKnownRO, cfloat2 & vf, const AmpIndex &idx) restrict(amp)
{
	auto fq = vf.x * tvKnownRO[idx];
	fq += tvUnknownRW(idx[0], idx[1], idx[2] - 1);
	fq += tvUnknownRW(idx[0], idx[1], idx[2] + 1);
	fq += tvUnknownRW(idx[0] - 1, idx[1], idx[2]);
	fq += tvUnknownRW(idx[0] + 1, idx[1], idx[2]);
	fq += tvUnknownRW(idx[0], idx[1] - 1, idx[2]);
	fq += tvUnknownRW(idx[0], idx[1] + 1, idx[2]);

	return fq / vf.y;
}

template<typename T>
inline void AmpPoisson3D<T>::jacobi(cfloat2 & vf)
{
	const auto tvUnknownRW = AmpRWTextureView<T>(dref(m_pTxPingpong));
	const auto tvUnknownRO = AmpTextureView<T>(dref(m_pTxUnknown));
	const auto tvKnownRO = AmpTextureView<T>(dref(m_pTxKnown));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvUnknownRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex idx) restrict(amp)
	{
		auto fq = vf.x * tvKnownRO[idx];
		fq += tvUnknownRO(idx[0], idx[1], idx[2] - 1);
		fq += tvUnknownRO(idx[0], idx[1], idx[2] + 1);
		fq += tvUnknownRO(idx[0] - 1, idx[1], idx[2]);
		fq += tvUnknownRO(idx[0] + 1, idx[1], idx[2]);
		fq += tvUnknownRO(idx[0], idx[1] - 1, idx[2]);
		fq += tvUnknownRO(idx[0], idx[1] + 1, idx[2]);

		tvUnknownRW.set(idx, fq / vf.y);
	}
	);

	// Swap
	m_pTxPingpong.swap(m_pTxUnknown);
}
