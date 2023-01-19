#include "imgui/imgui.h"

#include "ImGuiSceneNode.h"
#include "Mathematics/Utils.h"
#include "Mathematics/Constants.h"

#include <algorithm>

using candela::renderer::imgui::ImGuiSceneNode;
using candela::renderer::RendererTime;
using candela::scene::SceneNode;
using candela::scene::Scene;
using candela::mathematics::QuaternionToRotationXYZ;
using candela::mathematics::Vector3;
using candela::mathematics::Vector;

ImGuiSceneNode::ImGuiSceneNode(SceneNode &p_sceneNode, const RendererTime& rendererTime)
	: sceneNode(p_sceneNode), rendererTime(rendererTime), position{}, rotation{}, scale{ 1.f, 1.f, 1.f }, changed(), useModelCentre(true)
{
	Vector scale, rot, trans;
	DirectX::XMMatrixDecompose(&scale, &rot, &trans, sceneNode.Transform);
	DirectX::XMStoreFloat3(&this->scale, scale);
	DirectX::XMStoreFloat3(&this->position, trans);
	rotation = QuaternionToRotationXYZ(rot);
	
	for (auto& sceneChild : p_sceneNode.Children)
		children.emplace_back(*sceneChild, rendererTime);
}

void ImGuiSceneNode::drawUi()
{
	ImGui::PushID(this);
	if (rendererTime.isRunning())
	{
		ImGui::Text("Animated");
		ImGui::PopID();
		return;
	}

	if (sceneNode.isLeaf() || ImGui::TreeNode(sceneNode.NodeName.c_str()))
	{
		if (sceneNode.isLeaf())
			ImGui::Text(sceneNode.NodeName.c_str());

		changed = ImGui::DragFloat3("Position", &position.x, 0.01f);
		changed |= ImGui::DragFloat3("Rotation", &rotation.x, 0.01f);
		changed |= ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.f, 1000.f);
		changed |= ImGui::Checkbox("Rotate/scale around Model Centre", &useModelCentre);

		for (auto& nodeChild : children)
			nodeChild.drawUi();
		if (!sceneNode.isLeaf())
			ImGui::TreePop();
	}

	ImGui::PopID();

	if (changed || (useModelCentre && hasChanged()))
	{
		Vector3 localModelCentre{};
		if (useModelCentre)
			DirectX::XMStoreFloat3(&localModelCentre, sceneNode.getCentrePosition());
		sceneNode.Transform =
			  DirectX::XMMatrixTranslation(-localModelCentre.x, -localModelCentre.y, -localModelCentre.z)
			* DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
			* DirectX::XMMatrixRotationX(rotation.x)
			* DirectX::XMMatrixRotationY(rotation.y)
			* DirectX::XMMatrixRotationZ(rotation.z)
			* DirectX::XMMatrixTranslation(localModelCentre.x + position.x, localModelCentre.y + position.y, localModelCentre.z + position.z)
			;
	}
}

bool ImGuiSceneNode::hasChanged() const
{
	return changed || std::any_of(children.begin(), children.end(), [](const ImGuiSceneNode& s) { return s.hasChanged(); });
}
