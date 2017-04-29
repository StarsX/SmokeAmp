//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#define THREAD_BLOCK_X		8
#define THREAD_BLOCK_Y		8
#define THREAD_BLOCK_Z		8

#define REST_DENS			0.75

template<typename T>
inline AmpPoisson3D<T>::AmpPoisson3D()
{
}

template<typename T>
inline void AmpPoisson3D<T>::Init(cuint3 &vSimSize, const uint8_t bitWidth, AmpAcclView &acclView)
{
	Init(vSimSize.x, vSimSize.y, vSimSize.z, bitWidth, acclView);
}

template<>
inline void AmpPoisson3D<float>::Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth,
	const uint8_t bitWidth, AmpAcclView &acclView)
{
	const auto fWidth = static_cast<float>(iWidth);
	const auto fHeight = static_cast<float>(iHeight);
	const auto fDepth = static_cast<float>(iDepth);
	m_vSimSize = float3(fWidth, fHeight, fDepth);

	// Initialize data
	const auto uByteWidth = (bitWidth / 8u) * iWidth * iHeight * iDepth;
	auto vData = std::vector<byte>(uByteWidth);
	ZeroMemory(vData.data(), uByteWidth);

	// Create 3D textures
	m_pSrcKnown = std::make_shared<AmpTexture3D<float>>(iDepth, iHeight, iWidth, vData.data(), uByteWidth, bitWidth, acclView);
	m_pDstUnknown = std::make_shared<AmpTexture3D<float>>(iDepth, iHeight, iWidth, bitWidth, acclView);
	m_pSrcUnknown = nullptr;
}

template<typename T>
inline void AmpPoisson3D<T>::Init(const int32_t iWidth, const int32_t iHeight, const int32_t iDepth,
	const uint8_t bitWidth, AmpAcclView &acclView)
{
	const auto fWidth = static_cast<float>(iWidth);
	const auto fHeight = static_cast<float>(iHeight);
	const auto fDepth = static_cast<float>(iDepth);
	m_vSimSize = float3(fWidth, fHeight, fDepth);
	
	// Initialize data
	const auto uNumElement = sizeof(T) / sizeof(float);
	const auto uByteWidth = (bitWidth / 8u) * uNumElement * iWidth * iHeight * iDepth;
	auto vData = vbyte(uByteWidth);
	ZeroMemory(vData.data(), uByteWidth);

	// Create 3D textures
	m_pSrcKnown = make_shared<AmpTexture3D<T>>(iDepth, iHeight, iWidth, vData.data(), uByteWidth, bitWidth, acclView);
	m_pDstUnknown = make_shared<AmpTexture3D<T>>(iDepth, iHeight, iWidth, bitWidth, acclView);
	m_pSrcUnknown = make_shared<AmpTexture3D<T>>(iDepth, iHeight, iWidth, bitWidth, acclView);
}

template<typename T>
template<typename U>
inline void AmpPoisson3D<T>::ComputeDivergence(const AmpTexture3DView<U> &tvSource)
{
	const auto tvDstRW = AmpRWTexture3DView<T>(dref(m_pDstUnknown));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvDstRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex3D idx) restrict(amp)
	{
		tvDstRW.set(idx, Divergence3D(tvSource, idx));
	}
	);

	// Swap buffers
	SwapTextures();
}

template<>
inline void AmpPoisson3D<float>::SolvePoisson(cfloat2 &vf, const uint8_t uIteration)
{
	const auto tvUnknownRW = AmpRWTexture3DView<float>(*m_pDstUnknown);
	const auto tvKnownRO = AmpTexture3DView<float>(*m_pSrcKnown);

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvUnknownRW.extent.tile<THREAD_BLOCK_X, THREAD_BLOCK_Y, THREAD_BLOCK_Z>(),
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex3D idx) restrict(amp)
	{
		// Unordered Gauss-Seidel iteration
		for (concurrency::graphics::uint i = 0; i < 1024; ++i)
		{
			const auto fPressPrev = tvUnknownRW[idx];
			const auto fPress = gaussSeidel(tvUnknownRW, tvKnownRO, vf, idx);

			if (concurrency::fast_math::fabs(fPress - fPressPrev) < 0.0000001f) return;
			tvUnknownRW.set(idx, fPress);
		}
	}
	);

	// Swap buffers
	SwapTextures();
}

template<typename T>
inline void AmpPoisson3D<T>::SolvePoisson(cfloat2 &vf, const uint8_t uIteration)
{
	for (auto i = 0ui8; i < uIteration; ++i) jacobi(vf);

	// Swap buffers
	SwapTextures();
}

template<typename T>
template<typename U>
inline void AmpPoisson3D<T>::Advect(cfloat fDeltaTime, const AmpTexture3DView<U>& tvSource)
{
	const auto tvUnknownRW = AmpRWTexture3DView<T>(dref(m_pDstUnknown));
	const auto tvknownRO = AmpTexture3DView<T>(dref(m_pSrcKnown));

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
		tvUnknownRW.set(idx, tvknownRO.sample(vTex));
	}
	);

	// Swap buffers
	SwapTextures();
}

template<typename T>
inline void AmpPoisson3D<T>::SwapTextures(const bool bUnknown = false)
{
	if (bUnknown) m_pSrcUnknown.swap(m_pDstUnknown);
	else m_pSrcKnown.swap(m_pDstUnknown);
}

template<typename T>
inline float AmpPoisson3D<T>::gaussSeidel(const AmpRWTexture3DView<float> &tvUnknownRW,
	const AmpTexture3DView<float> &tvKnownRO, cfloat2 & vf, const AmpIndex3D &idx) restrict(amp)
{
	auto fq = vf.x * tvKnownRO[idx];
	fq += tvUnknownRW(idx[0], idx[1], idx[2] - 1);
	fq += tvUnknownRW(idx[0], idx[1], idx[2] + 1);
	fq += tvUnknownRW(idx[0], idx[1] - 1, idx[2]);
	fq += tvUnknownRW(idx[0], idx[1] + 1, idx[2]);
	fq += tvUnknownRW(idx[0] - 1, idx[1], idx[2]);
	fq += tvUnknownRW(idx[0] + 1, idx[1], idx[2]);

	return fq / vf.y;
}

template<typename T>
inline void AmpPoisson3D<T>::jacobi(cfloat2 & vf)
{
	const auto tvUnknownRW = AmpRWTexture3DView<T>(dref(m_pDstUnknown));
	const auto tvUnknownRO = AmpTexture3DView<T>(dref(m_pSrcUnknown));
	const auto tvKnownRO = AmpTexture3DView<T>(dref(m_pSrcKnown));

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		tvUnknownRW.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const AmpIndex3D idx) restrict(amp)
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

	// Swap buffers
	m_pSrcUnknown.swap(m_pDstUnknown);
}
