#include "ImGuiMaterial.h"

#include "imgui/imgui.h"

using candela::renderer::imgui::ImGuiMaterial;
using candela::scene::Material;
using std::size_t;

ImGuiMaterial::ImGuiMaterial(Material& material, size_t materialId)
	: material(material), materialId(materialId), changed()
{
}

void ImGuiMaterial::drawUi()
{
	ImGui::PushID(this);

	ImGui::Text("%d", materialId);
	changed = ImGui::DragFloat3("Diffuse", &material.Diffuse.x, 0.01f, 0.f, 1.f);
	if (material.isEmissive())
		changed |= ImGui::DragFloat3("Emissive", &material.Emissive.x, 0.01f, 0.01f, 1000.f);
	changed |= ImGui::DragFloat3("Tf", &material.TransmissiveFilter.x, 0.01f, 0.f, 1000.f);
	changed |= ImGui::DragFloat("Ni", &material.RefractiveIndex, 0.01f, 1.f, 1000.f);
	changed |= ImGui::DragFloat("d", &material.Dissolve, 0.01f, 0.f, 1.f);

	ImGui::PopID();
}

bool ImGuiMaterial::hasChanged() const
{
	return changed;
}
