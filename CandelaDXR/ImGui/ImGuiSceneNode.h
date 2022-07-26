#pragma once

#include <DirectXMath.h>

#include "Scene/Scene.h"
#include "Renderer/RendererTime.h"

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

		DirectX::XMFLOAT3 worldPosition;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 rotation;
		DirectX::XMFLOAT3 scale;

		bool changed;
	};
}