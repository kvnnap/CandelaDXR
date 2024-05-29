#include "OptixDenoiserShading.h"

#include <vector>

#include <d3dcompiler.h>
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"
#include "Mathematics/Types.h"
#include "DirectX/DxUtil.h"

#include <optix.h>
#include <optix_stubs.h>
#include <optix_function_table_definition.h>
#include <vector>
#include "Util/CudaHelper.h"
#include "Util/OptixHelper.h"

using std::make_unique;
using std::make_shared;
using std::uint32_t;
using std::uint16_t;
using std::vector;

using candela::renderer::OptixDenoiserShading;
//using candela::directx::RootSignatureManager;
using candela::directx::Resource;
//using candela::directx::DescriptorHeap;
using candela::directx::DXUtil;
using candela::mathematics::UVector2;

void OptixDenoiserShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;
	diffRadAccumulator = rRes->resourceManager->getNamedResource("diff_acc");
	specRadAccumulator = rRes->resourceManager->getNamedResource("spec_acc");
	
	// Init cuda stuff
	initOptix();
	createContext();
	createDenoiser();

	{
		//HANDLE sharedHandle{};
		//rRes->pDevice->CreateSharedHandle(TextureArray.Get(), NULL, GENERIC_ALL, 0, &sharedHandle);
		//const auto texAllocInfo = m_device->GetResourceAllocationInfo(m_nodeMask, 1, &texDesc);

		cudaExternalMemoryHandleDesc cuExtmemHandleDesc{};
		cuExtmemHandleDesc.type = cudaExternalMemoryHandleTypeD3D12Heap;
		cuExtmemHandleDesc.handle.win32.handle = NULL;
		cuExtmemHandleDesc.size = texAllocInfo.SizeInBytes;
		cuExtmemHandleDesc.flags = cudaExternalMemoryDedicated;
		CheckCudaErrors(cudaImportExternalMemory(&m_externalMemory, &cuExtmemHandleDesc));

		cudaExternalMemoryMipmappedArrayDesc cuExtmemMipDesc{};
		cuExtmemMipDesc.extent = make_cudaExtent(texDesc.Width, texDesc.Height, 0);
		cuExtmemMipDesc.formatDesc = cudaCreateChannelDesc<float4>();
		cuExtmemMipDesc.numLevels = 1;
		cuExtmemMipDesc.flags = cudaArraySurfaceLoadStore;

		cudaMipmappedArray_t cuMipArray{};
		CheckCudaErrors(cudaExternalMemoryGetMappedMipmappedArray(&cuMipArray, m_externalMemory, &cuExtmemMipDesc));

		cudaArray_t cuArray{};
		CheckCudaErrors(cudaGetMipmappedArrayLevel(&cuArray, cuMipArray, 0));

		cudaResourceDesc cuResDesc{};
		cuResDesc.resType = cudaResourceTypeArray;
		cuResDesc.res.array.array = cuArray;
		checkCudaErrors(cudaCreateSurfaceObject(&cuSurface, &cuResDesc));
	}
}

void OptixDenoiserShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
}

void OptixDenoiserShading::accept(IVisitor* visitor)
{
}

void OptixDenoiserShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
}

void OptixDenoiserShading::onResize()
{
}

std::uint32_t OptixDenoiserShading::getBufferUsage() const
{
	return BufferUsage::Diffuse | BufferUsage::Specular;
}

void OptixDenoiserShading::initOptix()
{
	// Initialise CUDA subsystem (trigger CUDA context creation and initialisation).
	CUDA(cudaFree(nullptr));

	int numCudaDevices;
	CUDA(cudaGetDeviceCount(&numCudaDevices));
	if (numCudaDevices == 0)
		throw std::runtime_error("No CUDA devices found!");

	OPTIX(optixInit());
}

void OptixDenoiserShading::createContext()
{
	const int deviceId = 0;
	CUDA(cudaSetDevice(deviceId));
	CUDA(cudaStreamCreate(&stream));

	//cudaDeviceProp deviceProps;
	//CUDA(cudaGetDeviceProperties(&deviceProps, deviceId));
	//std::cout << "GPU                 : " << deviceProps.name << std::endl;

	CUcontext cudaContext;
	CUresult ret = cuCtxGetCurrent(&cudaContext);
	if (ret != CUDA_SUCCESS) {
		std::cerr << "cuCtxGetCurrent failed. Error " << ret << "." << std::endl;
		throw std::runtime_error("Failed to create CUDA context!");
	}

	OPTIX(optixDeviceContextCreate(cudaContext, nullptr, &optixContext));
	//OPTIX(optixDeviceContextSetLogCallback(optixContext, optixLogCallback, nullptr, 4));
}

void OptixDenoiserShading::createDenoiser()
{
	auto& dim = rendererResources->winDimensions;

	OptixDenoiserOptions options = {};
	options.guideAlbedo = 0;
	options.guideNormal = 0;
	options.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;

	OptixDenoiserModelKind modelKind = OptixDenoiserModelKind::OPTIX_DENOISER_MODEL_KIND_HDR;

	OPTIX(optixDenoiserCreate(optixContext, modelKind, &options, &denoiser));

	OptixDenoiserSizes sizes;
	OPTIX(optixDenoiserComputeMemoryResources(denoiser, dim.x, dim.y, &sizes));

	denoiserStateSizeInBytes = sizes.stateSizeInBytes;
	denoiserScratchSizeInBytes = sizes.withoutOverlapScratchSizeInBytes;

	CUDA(cudaMalloc((void**)&denoiserState, denoiserStateSizeInBytes));
	CUDA(cudaMalloc((void**)&denoiserScratch, denoiserScratchSizeInBytes));

	OPTIX(optixDenoiserSetup(denoiser, stream, dim.x, dim.y, denoiserState, denoiserStateSizeInBytes, denoiserScratch,
		denoiserScratchSizeInBytes));
}