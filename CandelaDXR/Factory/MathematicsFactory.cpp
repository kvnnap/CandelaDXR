#include "MathematicsFactory.h"

#include "VectorFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::mathematics::factory::TransformComponentsFactory;
using candela::mathematics::TransformComponents;

unique_ptr<TransformComponents> TransformComponentsFactory::create() const
{
    return make_unique<TransformComponents>();
}

unique_ptr<TransformComponents> TransformComponentsFactory::create(const ConfigurationNode& config) const
{
    auto transformComponents = create();
    const auto& configObject = config.asObject();
    if (configObject.keyExists("Translation"))
        transformComponents->Translate = DirectX::XMLoadFloat3(&*Vector3Factory().create(configObject["Translation"]));
    if (configObject.keyExists("Scale"))
        transformComponents->Scale = DirectX::XMLoadFloat3(&*Vector3Factory().create(configObject["Scale"]));
    else
        transformComponents->Scale = DirectX::XMVectorSet(1.f, 1.f, 1.f, 0.f);
    if (configObject.keyExists("Rotation"))
        transformComponents->Rotate = DirectX::XMLoadFloat3(&*Vector3Factory().create(configObject["Rotation"]));
    return transformComponents;
}
