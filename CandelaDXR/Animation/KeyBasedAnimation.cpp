#include "KeyBasedAnimation.h"

#include <algorithm>
#include "Exception/Exception.h"

using std::uint32_t;
using std::vector;
using candela::animation::KeyFrame;
using candela::animation::KeyBasedAnimation;
using candela::mathematics::Matrix;
using candela::mathematics::Vector;

KeyBasedAnimation::KeyBasedAnimation()
	: duration()
{
}

Matrix KeyBasedAnimation::animate(uint32_t timeMs, const Vector& centreTranslation) const
{
	// Transform time to equivalent ticks
	if (ticksPerSecond > 0)
		timeMs *= static_cast<uint32_t>(ticksPerSecond / 1000.f);

	// Loop around
	timeMs %= duration;

	return
		  DirectX::XMMatrixScalingFromVector(interpolate(timeMs, scale, false, Vector{ 1.f, 1.f, 1.f, 1.f }))
		* DirectX::XMMatrixRotationQuaternion(interpolate(timeMs, rotation, true))
		* DirectX::XMMatrixTranslationFromVector(interpolate(timeMs, translation));
}

void KeyBasedAnimation::addRotation(const KeyFrame& keyFrame)
{
	addKeyFrame(keyFrame, rotation);
}

void KeyBasedAnimation::addTranslation(const KeyFrame& keyFrame)
{
	addKeyFrame(keyFrame, translation);
}

void KeyBasedAnimation::addScale(const KeyFrame& keyFrame)
{
	addKeyFrame(keyFrame, scale);
}

void KeyBasedAnimation::setTicksPerSecond(std::uint32_t tPerSecond)
{
	ticksPerSecond = tPerSecond;
}

static std::vector<KeyFrame>::const_iterator getNext(const vector<KeyFrame>& keyFrames, uint32_t timeMs)
{
	return std::lower_bound(keyFrames.begin(), keyFrames.end(), timeMs,
		[](const KeyFrame& keyFrame, uint32_t value)
		{
			return keyFrame.Time < value;
		}
	);
}

Vector KeyBasedAnimation::interpolate(uint32_t timeMs, const vector<KeyFrame>& keyFrames, bool isRotation, const Vector& defaultValue)
{
	// Empty, return default vector
	if (keyFrames.empty())
		return defaultValue;

	// Cannot interpolate, use constant value
	if (keyFrames.size() == 1)
		return keyFrames.front().Value;

	// Find the keyframe with value larger than current TimeMs
	auto next = getNext(keyFrames, timeMs);

	// if we reached the end, do not interpolate any longer
	if (next == keyFrames.end())
		return keyFrames.back().Value;

	if (next == keyFrames.begin())
		return keyFrames.front().Value;

	// Prev is guaranteed to be there
	auto prev = std::prev(next);

	// t ranges from 0 to 1
	auto t = static_cast<float>(timeMs - prev->Time) / (next->Time - prev->Time);

	return isRotation ?
		DirectX::XMQuaternionSlerp(DirectX::XMQuaternionNormalize(prev->Value), DirectX::XMQuaternionNormalize(next->Value), t) :
		DirectX::XMVectorLerp(prev->Value, next->Value, t);
}

void KeyBasedAnimation::addKeyFrame(const KeyFrame& keyFrame, std::vector<KeyFrame>& keyFrames)
{
	// Keep list sorted by
	auto it = getNext(keyFrames, keyFrame.Time);

	// Do not accept duplicates
	if (it != keyFrames.end() && it->Time == keyFrame.Time)
		ThrowException("KeyFrame with time '" + std::to_string(2) + "' already exists");

	keyFrames.insert(it, keyFrame);

	if (keyFrame.Time > duration)
		duration = keyFrame.Time;
}
