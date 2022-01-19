#pragma once

#include <string>

#include "ISceneLoader.h"
#include "Scene.h"

namespace candela::scene
{
	class WavefrontSceneLoader
		: public ISceneLoader
	{
	public:
		void loadScene() override;
		void setFilePath(const std::string& filePath);

	private:
		Scene *scene;
		std::string& filePath;
	};
}