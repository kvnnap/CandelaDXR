#include "Drawable.h"

using candela::renderer::Drawable;

Drawable::Drawable()
	: enabled(true)
{
}

bool Drawable::isEnabled() const
{
	return enabled;
}

void Drawable::setEnabled(bool p_enabled)
{
	enabled = p_enabled;
}

bool Drawable::shouldClearAccumulation() const
{
	return false;
}

std::uint32_t Drawable::getBufferUsage() const
{
	return BufferUsage::Radiance | BufferUsage::Diffuse | BufferUsage::Specular;
}
