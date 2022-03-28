#include "ImGuiMaterial.h"

#include "imgui/imgui.h"

using candela::renderer::imgui::ImGuiMaterial;
using candela::scene::Material;
using std::size_t;
using std::string;

ImGuiMaterial::ImGuiMaterial(Material& material, size_t materialId, const string &materialName)
	: material(material), materialId(materialId), materialName(materialName), changed(), majorChange()
{
}

void ImGuiMaterial::drawUi()
{
	ImGui::PushID(this);

	auto emm = material.Emissive;
	auto dis = material.Dissolve;

	ImGui::Text("%d - %s", materialId, materialName.c_str());
	changed = ImGui::DragFloat3("Diffuse", &material.Diffuse.x, 0.01f, 0.f, 1.f);
	changed |= ImGui::DragFloat3("Emissive", &material.Emissive.x, 0.01f, 0.f, 1000.f);
	changed |= ImGui::DragFloat3("Tf", &material.TransmissiveFilter.x, 0.01f, 0.f, 1000.f);
	changed |= ImGui::DragFloat("Ni", &material.RefractiveIndex, 0.01f, 1.f, 1000.f);
	changed |= ImGui::DragFloat("d", &material.Dissolve, 0.01f, 0.f, 1.f);

	majorChange = (emm.x == 0.f && emm.y == 0.f && emm.z == 0.f && material.isEmissive())
		|| ((emm.x > 0.f || emm.y > 0.f || emm.z > 0.f) && !material.isEmissive())
		|| (dis == 1.f && material.Dissolve < 1.f || dis < 1.f && material.Dissolve == 1.f);

	ImGui::PopID();
}

bool ImGuiMaterial::hasChanged() const
{
	return changed;
}

bool ImGuiMaterial::hasMajorChange() const
{
	return majorChange;
}
