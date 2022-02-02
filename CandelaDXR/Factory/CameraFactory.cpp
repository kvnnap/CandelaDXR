#include "CameraFactory.h"

#include "Environment/Environment.h"
#include "Renderer/Camera.h"

#include "VectorFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::renderer::Camera;
using candela::renderer::factory::CameraFactory;

using candela::mathematics::factory::UVector2Factory;
using candela::mathematics::factory::Vector3Factory;
using candela::mathematics::factory::Vector2Factory;

unique_ptr<Camera> CameraFactory::create() const
{
	return unique_ptr<Camera>();
}

unique_ptr<Camera> CameraFactory::create(const ConfigurationNode& config) const
{
    auto position = Vector3Factory().create(config["Position"]);
    auto direction = Vector3Factory().create(config["Direction"]);
    auto sensorSize = Vector2Factory().create(config["SensorDimensions"]);
    auto nearZ = config["Distance"].read<float>();
    auto farZ = config["MaxDistance"].read<float>();

    return make_unique<Camera>(
        DirectX::XMVectorSet(position->x, position->y, position->z, 1.f), 
        DirectX::XMVectorSet(direction->x, direction->y, direction->z, 0.f), 
        sensorSize->x, sensorSize->y, nearZ, farZ);
}
