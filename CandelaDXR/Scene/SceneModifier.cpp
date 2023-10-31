#include "SceneModifier.h"

#include <unordered_map>

using candela::scene::SceneModifier;
using candela::mathematics::Vector3;

SceneModifier::SceneModifier(Scene* scene)
	: scene(scene)
{
}

void SceneModifier::addProperty(const Property<float>& prop)
{
	floatProperties.emplace_back(prop);
}

void SceneModifier::addProperty(const Property<Vector3>& prop)
{
	vec3Properties.emplace_back(prop);
}

void SceneModifier::modifyScene()
{
	// Build Material Map
	std::unordered_map<std::string, Material*> materialMap;
	for (size_t i = 0; i < scene->getMaterials().size(); ++i)
		materialMap[scene->getMaterialName(i)] = &scene->getMaterials()[i];

	// Modify Materials
	for (const auto& p : floatProperties)
	{
		if (p.Type == "Material")
		{
			Material* mat = p.Name.empty() ? &scene->getMaterials().at(p.Id) : materialMap.at(p.Name);
			if (p.PropertyName == "RefractiveIndex")
				mat->RefractiveIndex = p.Data;
			else if (p.PropertyName == "Dissolve")
				mat->Dissolve = p.Data;
		}
	}

	// Modify Materials
	for (const auto& p : vec3Properties)
	{
		if (p.Type == "Material")
		{
			Material* mat = p.Name.empty() ? &scene->getMaterials().at(p.Id) : materialMap.at(p.Name);
			if (p.PropertyName == "Diffuse")
				mat->Diffuse = p.Data;
			else if (p.PropertyName == "Emissive")
				mat->Emissive = p.Data;
			else if (p.PropertyName == "Specular")
				mat->Specular = p.Data;
			else if (p.PropertyName == "TransmissiveFilter")
				mat->TransmissiveFilter = p.Data;
		}
	}

	scene->recalculateLightsAndFaceAttributes();
}


