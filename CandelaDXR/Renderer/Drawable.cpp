#include "Drawable.h"

using candela::renderer::Drawable;

Drawable::Drawable()
	: name("Unnamed"), enabled(true)
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

void Drawable::setName(const std::string& p_name)
{
	name = p_name;
}

const char* Drawable::getName() const
{
	return name.c_str();
}

bool Drawable::shouldClearAccumulation() const
{
	return false;
}

std::uint32_t Drawable::getBufferUsage() const
{
	return BufferUsage::Radiance | BufferUsage::Diffuse | BufferUsage::Specular;
}
