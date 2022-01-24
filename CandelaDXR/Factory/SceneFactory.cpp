#include "SceneFactory.h"
#include "Environment/Environment.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::scene::Scene;
using candela::scene::factory::SceneFactory;

SceneFactory::SceneFactory(Environment& env)
	: env(env)
{
}

unique_ptr<Scene> SceneFactory::create() const
{
	return make_unique<Scene>();
}

unique_ptr<Scene> SceneFactory::create(const ConfigurationNode& config) const
{
	return make_unique<Scene>();
}
