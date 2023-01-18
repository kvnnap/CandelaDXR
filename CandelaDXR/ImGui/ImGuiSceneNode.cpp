#include "imgui/imgui.h"

#include "ImGuiSceneNode.h"
#include "Mathematics/Constants.h"

#include <algorithm>

using candela::renderer::imgui::ImGuiSceneNode;
using candela::renderer::RendererTime;
using candela::scene::SceneNode;
using candela::scene::Scene;

ImGuiSceneNode::ImGuiSceneNode(SceneNode &p_sceneNode, const RendererTime& rendererTime)
	: sceneNode(p_sceneNode), rendererTime(rendererTime), position{}, rotation{}, scale{ 1.f, 1.f, 1.f }, changed()
{
	//XMStoreFloat3(&position, p_sceneNode.CentrePosition);
	//XMStoreFloat3(&worldPosition, DirectX::XMVectorNegate(p_sceneNode.CentrePosition));
	DirectX::XMVECTOR scale, rot, trans;
	DirectX::XMMatrixDecompose(&scale, &rot, &trans, sceneNode.Transform);
	//DirectX::XMQuaternionToAxisAngle(&axis, &angle, rot);
	//DirectX::XMVector3Normalize(axis);
	DirectX::XMStoreFloat3(&this->scale, scale);
	DirectX::XMStoreFloat3(&this->position, trans);
	float a = 2.f * (rot.m128_f32[3] * rot.m128_f32[0] + rot.m128_f32[1] * rot.m128_f32[2]);
	float b = 1.f - 2.f * (rot.m128_f32[0] * rot.m128_f32[0] + rot.m128_f32[1] * rot.m128_f32[1]);
	rotation.x = atan2f(a, b); //roll

	a = sqrtf(1.f + 2.f * (rot.m128_f32[3] * rot.m128_f32[1] - rot.m128_f32[0] * rot.m128_f32[2]));
	b = sqrtf(1.f - 2.f * (rot.m128_f32[3] * rot.m128_f32[1] - rot.m128_f32[0] * rot.m128_f32[2]));
	rotation.y = 2.f * atan2f(a, b) - mathematics::constants::PiOver2; // pitch
	
	a = 2.f * (rot.m128_f32[3] * rot.m128_f32[2] + rot.m128_f32[0] * rot.m128_f32[1]);
	b = 1.f - 2.f * (rot.m128_f32[1] * rot.m128_f32[1] + rot.m128_f32[2] * rot.m128_f32[2]);
	rotation.z = atan2(a, b); // yaw
	
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
		
		for (auto& nodeChild : children)
			nodeChild.drawUi();
		if (!sceneNode.isLeaf())
			ImGui::TreePop();
	}

	ImGui::PopID();

	// TODO: Handle initial rotation and scaling as well
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
	return changed || std::any_of(children.begin(), children.end(), [](const ImGuiSceneNode& s) { return s.hasChanged(); });
}
