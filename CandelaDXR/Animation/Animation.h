#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "IAnimation.h"
#include "Mathematics/TransformComponents.h"

namespace candela::animation
{
	using MeshState = mathematics::TransformComponents;

	struct Transition
	{
		std::size_t MeshStateId; // Transition to this state
		std::uint32_t TimeMS; // Duration of this transition
		std::uint32_t CumulativeTimeMS; // Filled automatically - Total time from start including this transition (Cumulative end time)

		std::uint32_t getStartCumulativeTimeMS() const; // Cumulative Start Time
	};

	class Animation
		: public IAnimation
	{
	public:
		Animation();
		mathematics::Matrix animate(std::uint32_t timeMs, const mathematics::Vector& centreTranslation = {}) const override;
		std::uint32_t getTotalTimeMs() const;

		void setInitialMeshStateId(std::size_t initialMeshStateId);
		void setTranslationAbsolute(bool translationAbsolute);
		void addMeshState(const MeshState& meshState);
		void addTransition(const Transition& transition);
	private:

		std::vector<MeshState> meshStates;
		std::vector<Transition> transitions;
		std::size_t initialMeshStateId;
		bool translationAbsolute;
	};
}