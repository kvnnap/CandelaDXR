#pragma once

#include <DirectXMath.h>

#include "Scene/Scene.h"

namespace candela::renderer::imgui
{
	class ImGuiSceneNode
	{
	public:
		ImGuiSceneNode(scene::SceneNode &p_sceneNode, const scene::Scene& scene);

		void drawUi();

		bool hasChanged() const;
	private:
		scene::SceneNode &sceneNode;

		DirectX::XMFLOAT3 worldPosition;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 rotation;
		DirectX::XMFLOAT3 scale;

		bool changed;
	};
}