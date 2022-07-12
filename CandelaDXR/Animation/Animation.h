#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Mathematics/Types.h"

namespace candela::animation
{
	struct MeshState
	{
		DirectX::XMVECTOR Translate;
		DirectX::XMVECTOR Scale;
		DirectX::XMVECTOR Rotate;
	};

	struct Transition
	{
		std::size_t MeshStateId; // Transition to this state
		std::uint32_t TimeMS; // Duration of this transition
		std::uint32_t CumulativeTimeMS; // Filled automatically - Total time from start including this transition
	};

	class Animation
	{
	public:
		mathematics::Matrix animate(std::uint32_t timeMs) const;
		std::uint32_t getTotalTimeMs() const;

		void setMeshName(const std::string& meshName);
		void setWorldCentrePosition(const DirectX::XMVECTOR& centreTranslation);
		void setInitialMeshStateId(std::size_t initialMeshStateId);
		void addMeshState(const MeshState& meshState);
		void addTransition(const Transition& transition);

		const std::string& getMeshName() const;
	private:

		std::vector<MeshState> meshStates;
		std::vector<Transition> transitions;
		std::string meshName;
		std::size_t initialMeshStateId;

		DirectX::XMVECTOR centreTranslation;
	};
}