#include "RendererFactory.h"

#include "Environment/Environment.h"
#include "Renderer/Renderer.h"
#include "Renderer/IDrawable.h"
#include "Animation/Animation.h"
#include "Exception/Exception.h"
#include "VectorFactory.h"

#include <vector>
#include <cstdint>

using std::unique_ptr;
using std::make_unique;
using std::vector;
using std::string;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::renderer::IRenderer;
using candela::renderer::Renderer;
using candela::renderer::AnimationRecord;
using candela::renderer::ITransform;
using candela::animation::Animation;
using candela::animation::AnimationSequencer;
using candela::renderer::factory::RendererFactory;
using candela::renderer::Camera;
using candela::mathematics::factory::UVector2Factory;

RendererFactory::RendererFactory(Environment& env)
	: env(env)
{
}

unique_ptr<IRenderer> RendererFactory::create() const
{
	return unique_ptr<Renderer>();
}

unique_ptr<IRenderer> RendererFactory::create(const ConfigurationNode& config) const
{
	auto scene = &env.getSceneManager().getInstanceManager().get(config["Scene"]);
	
	auto& cameraInstMg = env.getCameraManager().getInstanceManager();
	auto cameraName = config["Camera"].read<string>();
	Camera* camera{};
	if (cameraInstMg.exists(cameraName))
	{
		camera = &cameraInstMg.get(cameraName);
	}
	else // Fallback to scene camera
	{
		const auto& v = scene->getCameras();
		auto it = std::find_if(v.begin(), v.end(), [&cameraName](const candela::scene::Scene::CameraNode& obj) { return obj.Camera.getName() == cameraName; });
		if (it == v.end())
			ThrowException("Could not find camera: " + cameraName);
		auto cameraId = cameraInstMg.registerItem(make_unique<Camera>(it->Camera));
		camera = &cameraInstMg.get(cameraId);
	}

	const auto& confObject = config.asObject();

	// Gen animation mapping
	vector<AnimationRecord> animationMapping;
	if (confObject.keyExists("Animations"))
	{
		for (const auto& animConfig : config["Animations"].asList())
		{
			auto anim = &env.getAnimationManager().getInstanceManager().get(animConfig["Name"]);
			for (const auto& targetNode : animConfig["Targets"].asList())
			{
				auto targetName = targetNode.read<string>();
				if (config["Camera"].read<string>() == targetName)
					animationMapping.push_back({ camera, anim, targetName, true });
				for (auto node : scene->getSceneGraph().getAllNodes())
					if (node->NodeName == targetName)
						animationMapping.push_back({ node, anim, targetName, true });
			}
		}
	}

	auto dim = *UVector2Factory().create(config["WindowDimensions"]);
	auto& drawablesConfig = config["Drawables"].asList();
	bool debugEnabled = false;
	bool breakEnabled = false;
	bool vsync = false;
	bool exitOnAnimCompl = false;
	bool shaderAccumulation = true;
	std::uint32_t adapterIndex = 0;
	if (confObject.keyExists("AdapterIndex"))
		adapterIndex = config["AdapterIndex"].read<std::uint32_t>();
	if (confObject.keyExists("Debug"))
		debugEnabled = config["Debug"].read<bool>();
	if (confObject.keyExists("Break"))
		breakEnabled = config["Break"].read<bool>();
	if (confObject.keyExists("VSync"))
		vsync = config["VSync"].read<bool>();
	if (confObject.keyExists("ExitOnAnimationCompletion"))
		exitOnAnimCompl = config["ExitOnAnimationCompletion"].read<bool>();
	if (confObject.keyExists("ShaderAccumulation"))
		shaderAccumulation = config["ShaderAccumulation"].read<bool>();

	std::vector<IDrawable*> drawables;
	for (auto& drawableConfig : drawablesConfig)
		drawables.push_back(&env.getDrawableManager().getInstanceManager().get(drawableConfig));
	auto renderer = make_unique<Renderer>(scene, camera, dim, std::move(drawables), adapterIndex, debugEnabled, breakEnabled, vsync, exitOnAnimCompl, shaderAccumulation);
	renderer->setAnimationRecords(std::move(animationMapping));

	// Animation Seq
	if (confObject.keyExists("AnimationSequencer"))
	{
		const auto& animSeqConf = config["AnimationSequencer"].asObject();
		auto enabled = animSeqConf["Enabled"].read<bool>();
		auto framesPerAnimation = animSeqConf["FramesPerAnimation"].read<std::uint32_t>();
		auto timeDeltaMs = animSeqConf["TimeDeltaMs"].read<std::uint32_t>();
		auto maxTimeMs = animSeqConf["MaxTimeMs"].read<std::uint32_t>();
		auto& animSeq = renderer->getAnimationSequencer();
		animSeq.setFramesPerAnimation(framesPerAnimation);
		animSeq.setTimeDeltaMs(timeDeltaMs);
		animSeq.setMaxTimeMs(maxTimeMs);
		animSeq.setEnabled(enabled);
	}

	// Chain
	if (confObject.keyExists("Chain"))
	{
		auto chain = &env.getChainManager().getInstanceManager().get(config["Chain"]);
		renderer->setChain(chain);
	}

	// Frames to grab
	if (confObject.keyExists("FramesToGrab"))
	{
		std::vector<std::uint64_t> framesToGrab;
		for (const auto& frame : config["FramesToGrab"].asList())
			framesToGrab.push_back(frame.read<std::uint64_t>());
		renderer->setFramesToGrab(std::move(framesToGrab));
	}

	// Buffer names to grab on screenshot
	if (confObject.keyExists("BuffersToGrab"))
	{
		std::vector<std::string> buffersToGrab;
		for (const auto& frame : config["BuffersToGrab"].asList())
			buffersToGrab.push_back(frame.read<std::string>());
		renderer->setBuffersToGrab(std::move(buffersToGrab));
	}

	return renderer;
}
