#pragma once

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

		bool shouldClearAccumulation() const override;
	private:
		bool enabled;
	};
}