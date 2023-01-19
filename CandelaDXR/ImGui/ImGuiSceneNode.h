#pragma once

#include "Scene/Scene.h"
#include "Renderer/RendererTime.h"
#include "Mathematics/Types.h"

namespace candela::renderer::imgui
{
	class ImGuiSceneNode
	{
	public:
		ImGuiSceneNode(scene::SceneNode &p_sceneNode, const RendererTime& rendererTime);

		void drawUi();

		bool hasChanged() const;
	private:
		scene::SceneNode &sceneNode;
		const RendererTime& rendererTime;
		std::vector<ImGuiSceneNode> children;

		mathematics::Vector3 position;
		mathematics::Vector3 rotation;
		mathematics::Vector3 scale;

		bool changed;
		bool useModelCentre;
	};
}