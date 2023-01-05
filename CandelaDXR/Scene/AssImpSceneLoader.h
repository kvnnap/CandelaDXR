#pragma once

#include <string>

#include "ISceneLoader.h"
#include "Scene.h"

namespace candela::scene
{
	class AssImpSceneLoader
		: public ISceneLoader
	{
	public:
		AssImpSceneLoader(Scene* scene);
		void loadScene() override;
		void setFilePath(const std::string& filePath);
		void setAlwaysComputeNormals(bool value);
	private:
		Scene* scene;
		std::string filePath;

		bool alwaysComputeNormals = false;
	};
}
