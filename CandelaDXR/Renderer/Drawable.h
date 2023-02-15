#pragma once

#include <string>

#include "IDrawable.h"

namespace candela::renderer
{
	class Drawable
		: public IDrawable
	{
	public:
		Drawable();

		bool isEnabled() const override;
		void setEnabled(bool p_enabled) override;

		void setName(const std::string& p_name);
		const char* getName() const override;

		bool shouldClearAccumulation() const override;
		std::uint32_t getBufferUsage() const override;
	private:
		std::string name;
		bool enabled;
	};
}