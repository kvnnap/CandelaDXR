#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <queue>

#include "DirectX/Types.h"
#include <dxgi1_6.h>

#include "feanor/core/io/keyboard.h"
#include "feanor/core/io/mouse.h"
#include "Window/Window.h"
#include "DirectX/DxgiInfoManager.h"
#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/Resource.h"
#include "DirectX/TimeStampQuery.h"
#include "FpsCounter.h"
#include "RendererTime.h"
#include "Scene/Scene.h"
#include "IRenderer.h"
#include "IDrawable.h"
#include "Camera.h"
#include "Chain/IChain.h"

#include "ImGui/ImGuiManager.h"
#include "RendererResources.h"
#include "ITransform.h"
#include "Animation/IAnimation.h"
#include "Animation/AnimationSequencer.h"
#include "ExternalObjectDebugShading.h"

#include "feanor/anvil/core/anvil.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	struct AnimationRecord
	{
		ITransform* transform;
		animation::IAnimation* animation;
		std::string name;
		bool enabled;
	};

	class Renderer
		: public IRenderer
	{
	public:
		struct PostProcParams
		{
			std::uint32_t Flags;
			float Exposure;
		};

		Renderer(scene::Scene *scene, Camera *camera, 
			const mathematics::UVector2& windowDimensions, std::vector<IDrawable*> drawables, 
			std::uint32_t adapterIndex, bool debugEnabled, bool breakEnabled, bool vsync, bool exitOnAnimCompl
			, bool shaderAccumulation);
		~Renderer();

		void init() override;
		void renderFrame() override;
		void setShaderAccumulation(bool shaderAccumulation);
		bool getShaderAccumulation() const;
		const PostProcParams& getPostProcParams() const;
		void setPostProcParams(const PostProcParams& p_ppParams);

		const std::vector<directx::ProfileItem>& getProfilingData() const;

		void setCameraCopy(const Camera& camera);
		const scene::Scene& getScene() const;

		void setChain(chain::CFList* chain);
		void checkAndSetFullScreenMode();

		// Promote to interface?
		RendererTime& getRendererTime();
		void setAnimationRecords(std::vector<AnimationRecord>&& animationRecords);
		std::vector<AnimationRecord>& getAnimationRecords();

		animation::AnimationSequencer& getAnimationSequencer();

		void setFramesToGrab(std::vector<std::uint64_t>&& framesToGrab);
		void setBuffersToGrab(std::vector<std::string>&& buffersToGrab);

		// Anvil
		ANVIL_CODE_RAW(
			bool isAnvilEnabled() const;
			void setAnvilEnabled(bool);
		)

	private:
		// Accum Resource Enum
		enum AccumResource : std::uint32_t
		{
			RTV8BitBackBuffer = 0,
			RTVRad = 1,
			RTVDiff = 2,
			RTVSpec = 3,
			RTVCaus = 4,
			RadAccumulator = 5,
			DiffAccumulator = 6,
			SpecAccumulator = 7,
			CausAccumulator = 8
		};

		// Accum Const Buff
		struct AccumConstBuff
		{
			AccumResource InIndex[4];
			AccumResource OutIndex[4];
			std::uint32_t PairCount;
			std::uint32_t Flags;
			float Exposure;
		};

		template<class T>
		using ComPtrVec = std::vector<wrl::ComPtr<T>>;
		
		using ResPtrVec = std::vector<std::shared_ptr<directx::Resource>>;

		void initSceneResources();
		void initShaders();
		ChangeEvent_t setSceneCameraFromNodeTransform();
		void createShaderResources();
		void updateCamera();
		void resize();
		void refreshMaterialResources();
		void bindComputePipeline();
		void dispatchCompute(wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, const AccumConstBuff& c32Data);
		bool isRecording() const;
		void recordingChange();
		directx::DXResource& getTempResource();
		std::vector<DirectX::XMFLOAT3X4> getMatrices();
		std::vector<DirectX::XMFLOAT3X3> getNormalMatrices();
		std::vector<scene::Light> getTransformedExternalLights();
		LRESULT wndCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		// Basic I/O and Window
		feanor::io::Keyboard keyboard;
		feanor::io::Mouse mouse;
		std::unique_ptr<ui::Window> window;
		mathematics::UVector2 windowDimensions;

		// DirectX
		const std::uint32_t adapterIndex;
		wrl::ComPtr<ID3D12Device> pDevice;
		std::unique_ptr<directx::CommandQueue> commandQueue;
		wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList;

		static constexpr UINT NumBackBuffers = 2;
		wrl::ComPtr<IDXGIFactory> dxgiFactory;
		wrl::ComPtr<IDXGISwapChain> pSwapChain;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		directx::Resource * pRTV8Bit; // This is the 8-bit target that will then be copied to swap-chain
		directx::Resource * pRTVRad, * pRTVDiff, * pRTVSpec, *pRTVCaus; // This is the target for drawables (32-bit)
		directx::Resource * pRadAccumulator, * pDiffAccumulator, * pSpecAccumulator, * pCausAccumulator; // 32-bit
		ResPtrVec pRTVBackBuffers; // Buffers retrieved from swap-chain (these are 8-bit)

		// ImGui Manager - is also a drawable
		imgui::ImGuiManager imguiManager;

		// Constants and integral values
		UINT rtvDescriptorSize;
		UINT currentBackBufferIndex;
		std::vector<uint64_t> frameFenceValues;

		// Stats
		FpsCounter fpsCounter;
		RendererTime rendererTime;
		std::uint64_t frameGrabCounter;

		// Animation
		std::vector<AnimationRecord> animationRecords;
		animation::AnimationSequencer animationSequencer;

		// Scene
		scene::Scene *scene;
		Camera *camera;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> specularBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		wrl::ComPtr<ID3D12Resource> normalMatrices;
		wrl::ComPtr<ID3D12Resource> externalLights;
		std::vector<directx::Resource> textures;

		// Chain
		chain::CFList* chain;

		// Data dump stuff
		std::vector<std::uint64_t> frameNumbersForGrab;
		std::vector<std::string> buffersToGrab;

		// Shaders
		std::shared_ptr<directx::RootSignatureManager> computeRSM;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;
		wrl::ComPtr<ID3D12DescriptorHeap> computeDescriptorHeap;

		//
		std::queue<std::function<void()>> postFrameActions;

		// Queries
		directx::TimeStampQuery timeStampQuery[NumBackBuffers];

		// TEST AREA
		std::vector<IDrawable*> drawables;
		std::vector<std::unique_ptr<IResource>> resources;

		// To pass
		RendererResources rendererResources;
		PostProcParams ppParams;

		// Debug
		std::unique_ptr<directx::DxgiInfoManager> dxgiInfoManager;
		const bool debugEnabled;
		// Only enable this when a debugger is attached
		// otherwise on DX error, program calls abort/exit
		const bool breakEnabled;
		const bool vsync;
		const bool exitOnAnimationCompletion;
		bool isFullScreen;
		bool tearingSupported;
		bool shaderAccumulation;

		ExternalObjectDebugShading eoDebug;

		// Anvil
		ANVIL_CODE_RAW(
			bool anvilEnabled = true;
			bool anvilParallel = false;
			void initAnvil();

			struct AnvilCapture
			{
				std::shared_ptr<feanor::anvil::Entity> entity;
				RadianceBuffer radBuffer;
			};

			std::vector<AnvilCapture> bufferEntities;
		)
	};
}