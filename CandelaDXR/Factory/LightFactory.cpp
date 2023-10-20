#include "LightFactory.h"
#include "VectorFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::scene::Light;
using candela::scene::factory::LightFactory;
using candela::mathematics::factory::Vector3Factory;
using candela::mathematics::factory::Vector2Factory;
using candela::mathematics::Vector;
using candela::mathematics::Vector3;

unique_ptr<Light> LightFactory::create() const
{
	return make_unique<Light>();
}

unique_ptr<Light> LightFactory::create(const ConfigurationNode& config) const
{
	auto light = make_unique<Light>();

	const auto& configObject = config.asObject();

	auto setVec3 = [&configObject](const std::string& name, Vector3& vec3Out)
	{
		if (!configObject.keyExists(name))
			return;
		vec3Out = *Vector3Factory().create(configObject[name]);
	};

	auto setVec = [&configObject](const std::string& name, float w, Vector& vecOut)
	{
			if (!configObject.keyExists(name))
				return;
			auto val = Vector3Factory().create(configObject[name]);
			vecOut = DirectX::XMVectorSet(val->x, val->y, val->z, w);
	};

	const auto& typeName = config["LightType"].read<std::string>();
	std::uint32_t typeId{};
	if (typeName == "Directional")
		typeId = 1;
	else if (typeName == "Point")
		typeId = 2;
	else if (typeName == "Spot")
		typeId = 3;
	else if (typeName == "Ambient")
		typeId = 4;
	else if (typeName == "Area")
		typeId = 5;

	light->Type = typeId;
	light->Attenuation = Vector3(1.f, 1.f, 1.f);

	setVec("Position", 1.f, light->Position);
	if (configObject.keyExists("Direction"))
	{
		setVec("Direction", 0.f, light->Direction);
		light->Direction = DirectX::XMVector3Normalize(light->Direction);
	}
	setVec("Up", 0.f, light->Up);

	setVec3("Attenuation", light->Attenuation);
	setVec3("Diffuse", light->Diffuse);
	setVec3("Specular", light->Specular);

	if (configObject.keyExists("InnerConeAngle"))
		light->InnerConeAngle = 1.f - cosf(config["InnerConeAngle"].read<float>());

	if (configObject.keyExists("OuterConeAngle"))
		light->OuterConeAngle = 1.f - cosf(config["OuterConeAngle"].read<float>());

	if (configObject.keyExists("AreaDimensions"))
		light->AreaDimensions = *Vector2Factory().create(config["AreaDimensions"]);

	return light;
}
