#pragma once

#include <string>

#include "ISceneLoader.h"
#include "Scene.h"

namespace candela::scene
{
	struct AssImpOffsets
	{
		std::size_t mesh;
		std::size_t camera;
		std::size_t light;
	};

	class AssImpSceneLoader
		: public ISceneLoader
	{
	public:
		AssImpSceneLoader(Scene* scene);
		void loadScene() override;
		void setFilePath(const std::string& filePath);
		void setAlwaysComputeNormals(bool value);
	private:
		Scene* scene{};
		std::string filePath;

		AssImpOffsets offsets{};
		
		bool alwaysComputeNormals = false;
	};
}
