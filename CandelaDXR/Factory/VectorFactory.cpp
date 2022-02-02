#include "VectorFactory.h"

using std::unique_ptr;
using std::make_unique;
using feanor::configuration::ConfigurationNode;
using candela::mathematics::factory::Vector3Factory;
using candela::mathematics::factory::Vector2Factory;
using candela::mathematics::factory::UVector2Factory;
using candela::mathematics::Vector3;
using candela::mathematics::Vector2;
using candela::mathematics::UVector2;

unique_ptr<Vector3> Vector3Factory::create() const
{
    return make_unique<Vector3>();
}

unique_ptr<Vector3> Vector3Factory::create(const ConfigurationNode& vectorNode) const
{
    auto v = create();
    v->x = vectorNode["x"].read<float>();
    v->y = vectorNode["y"].read<float>();
    v->z = vectorNode["z"].read<float>();
    return v;
}

unique_ptr<Vector2> Vector2Factory::create() const
{
    return make_unique<Vector2>();
}

unique_ptr<Vector2> Vector2Factory::create(const ConfigurationNode& vectorNode) const
{
    auto v = create();
    v->x = vectorNode["x"].read<float>();
    v->y = vectorNode["y"].read<float>();
    return v;
}

unique_ptr<UVector2> UVector2Factory::create() const
{
    return make_unique<UVector2>();
}

unique_ptr<UVector2> UVector2Factory::create(const ConfigurationNode& vectorNode) const
{
    auto v = create();
    v->x = vectorNode["x"].read<std::uint32_t>();
    v->y = vectorNode["y"].read<std::uint32_t>();
    return v;
}
