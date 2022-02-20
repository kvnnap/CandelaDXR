#include "ImGuiSceneNode.h"

#include "imgui/imgui.h"

using candela::renderer::imgui::ImGuiSceneNode;
using candela::scene::SceneNode;
using candela::scene::Scene;

ImGuiSceneNode::ImGuiSceneNode(SceneNode &p_sceneNode, const Scene &scene)
	: sceneNode(p_sceneNode), position{}, rotation{}, scale{ 1.f, 1.f, 1.f }, changed()
{
	// Loop through vertices
	const auto &indexedSpan = scene.getMeshIndexedSpan(sceneNode.GroupName);
	const auto& indices = scene.getIndices();
	const auto& vertices = scene.getVertices();
	DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.f, 0.f, 0.f, 0.f);
	for (std::size_t i = indexedSpan.Start; i < indexedSpan.Start + indexedSpan.Size; ++i)
	{
		const auto& vertex = vertices[indices[i]];
		pos = DirectX::XMVectorAdd(pos, DirectX::XMLoadFloat3(&vertex));
	}

	float div = static_cast<float>(indexedSpan.Size);
	pos = DirectX::XMVectorDivide(pos, DirectX::XMVectorSet(div, div, div, div));

	XMStoreFloat3(&position, pos);
	XMStoreFloat3(&worldPosition, DirectX::XMVectorNegate(pos));
}

void ImGuiSceneNode::drawUi()
{
	ImGui::PushID(this);

	ImGui::Text(sceneNode.NodeName.c_str());
	changed =
		ImGui::DragFloat3("Position", &position.x, 0.01f)
		| ImGui::DragFloat3("Rotation", &rotation.x, 0.01f)
		| ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.f, 1000.f);

	ImGui::PopID();

	if (changed)
		sceneNode.Transform = 
		  DirectX::XMMatrixTranslation(worldPosition.x, worldPosition.y, worldPosition.z)
		* DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
		* DirectX::XMMatrixRotationX(rotation.x)
		* DirectX::XMMatrixRotationY(rotation.y)
		* DirectX::XMMatrixRotationZ(rotation.z)
		* DirectX::XMMatrixTranslation(position.x, position.y, position.z)
		;
}

bool ImGuiSceneNode::hasChanged() const
{
	return changed;
}
