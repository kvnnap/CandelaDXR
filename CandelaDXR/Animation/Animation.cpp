#include <algorithm>

#include "Animation.h"

using std::uint32_t;
using std::size_t;
using candela::mathematics::Matrix;
using candela::mathematics::Vector;
using candela::animation::Animation;
using candela::animation::Transition;
using candela::animation::MeshState;

uint32_t Transition::getStartCumulativeTimeMS() const
{
	return CumulativeTimeMS - TimeMS;
}

Animation::Animation()
	: initialMeshStateId(), translationAbsolute()
{}

Matrix Animation::animate(uint32_t timeMs, const Vector& centreTranslation) const
{
	// Cycle the animation
	timeMs %= getTotalTimeMs();

	// Find the current transition (binary search)
	auto lower = std::lower_bound(transitions.begin(), transitions.end(), timeMs,
		[] (const Transition& transition, uint32_t value)
		{
			return transition.CumulativeTimeMS < value;
		}
	);

	const auto& current = *lower;
	auto currentIndex = std::distance(transitions.begin(), lower);

	// Grab the previous and current state
	const MeshState& statePrev = currentIndex == 0 ? meshStates[initialMeshStateId] : meshStates[transitions[currentIndex - 1].MeshStateId];
	const MeshState& stateCurrent = meshStates[lower->MeshStateId];

	// Get cumulative time prior to starting the current transition, calculate t in [0,1]
	auto t = static_cast<float>(timeMs - current.getStartCumulativeTimeMS()) / current.TimeMS;

	// Perform a linear interpolation of the prevState towards the currentState by using t
	MeshState computedMS{ statePrev };
	computedMS.lerp(stateCurrent, t);
	return computedMS.transform(centreTranslation, translationAbsolute);
}

uint32_t Animation::getTotalTimeMs() const
{
	return transitions.empty() ? 0u : transitions.back().CumulativeTimeMS;
}

void Animation::setInitialMeshStateId(std::size_t initialMeshStateId)
{
	this->initialMeshStateId = initialMeshStateId;
}

void Animation::setTranslationAbsolute(bool p_translationAbsolute)
{
	translationAbsolute = p_translationAbsolute;
}

void Animation::addMeshState(const MeshState& meshState)
{
	meshStates.push_back(meshState);
}

void Animation::addTransition(const Transition& transition)
{
	auto prevAccumTime = getTotalTimeMs();
	auto& trans = transitions.emplace_back(transition);
	trans.CumulativeTimeMS = prevAccumTime + trans.TimeMS;
}

