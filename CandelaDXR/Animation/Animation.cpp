#include <algorithm>

#include "Animation.h"

using std::uint32_t;
using std::size_t;
using candela::mathematics::Matrix;
using candela::animation::Animation;
using candela::animation::Transition;
using candela::animation::MeshState;

Matrix Animation::animate(std::uint32_t timeMs, const DirectX::XMVECTOR& centreTranslation) const
{
	timeMs %= getTotalTimeMs();

	auto lower = std::lower_bound(transitions.begin(), transitions.end(), timeMs,
		[] (const Transition& transition, uint32_t value)
		{
			return transition.CumulativeTimeMS < value;
		}
	);

	const auto& current = *lower;
	auto currentIndex = std::distance(transitions.begin(), lower);

	const MeshState& statePrev = currentIndex == 0 ? meshStates[initialMeshStateId] : meshStates[transitions[currentIndex - 1].MeshStateId];
	const MeshState& stateCurrent = meshStates[lower->MeshStateId];

	auto t = static_cast<float>(timeMs - (current.CumulativeTimeMS - current.TimeMS)) / current.TimeMS;
	auto translate = DirectX::XMVectorLerp(statePrev.Translate, stateCurrent.Translate, t);
	auto rotate = DirectX::XMVectorLerp(statePrev.Rotate, stateCurrent.Rotate, t);
	auto scale = DirectX::XMVectorLerp(statePrev.Scale, stateCurrent.Scale, t);

	if (!translationAbsolute)
		translate = DirectX::XMVectorAdd(centreTranslation, translate);

	return DirectX::XMMatrixTranslationFromVector(DirectX::XMVectorNegate(centreTranslation))
		* DirectX::XMMatrixScalingFromVector(scale)
		* DirectX::XMMatrixRotationX(rotate.m128_f32[0])
		* DirectX::XMMatrixRotationY(rotate.m128_f32[1])
		* DirectX::XMMatrixRotationZ(rotate.m128_f32[2])
		* DirectX::XMMatrixTranslationFromVector(translate);
}

uint32_t Animation::getTotalTimeMs() const
{
	return transitions.back().CumulativeTimeMS;
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
	auto prevAccumTime = transitions.empty() ? 0u : transitions.back().CumulativeTimeMS;
	transitions.push_back(transition);
	transitions.back().CumulativeTimeMS = prevAccumTime + transitions.back().TimeMS;
}

