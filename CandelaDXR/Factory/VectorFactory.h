#pragma once

#include "feanor/core/factory/factory.h"

#include "Mathematics/Types.h"

namespace candela::mathematics::factory
{
	class Vector3Factory
		: public feanor::factory::Factory<Vector3>
	{
	public:
		std::unique_ptr<Vector3> create() const override;
		std::unique_ptr<Vector3> create(const feanor::configuration::ConfigurationNode& config) const override;
	};

	class Vector2Factory
		: public feanor::factory::Factory<Vector2>
	{
	public:
		std::unique_ptr<Vector2> create() const override;
		std::unique_ptr<Vector2> create(const feanor::configuration::ConfigurationNode& config) const override;
	};

	class UVector2Factory
		: public feanor::factory::Factory<UVector2>
	{
	public:
		std::unique_ptr<UVector2> create() const override;
		std::unique_ptr<UVector2> create(const feanor::configuration::ConfigurationNode& config) const override;
	};
}
