#pragma once

#include <vector>

#include "IAnimation.h"

namespace candela::animation
{
	struct KeyFrame {
		mathematics::Vector Value;
		std::uint32_t Time; // The time at which this value applies
	};

	// Used for Assimp based node-animation
	class KeyBasedAnimation
		: public IAnimation
	{
	public:
		KeyBasedAnimation();
		mathematics::Matrix animate(std::uint32_t timeMs, const mathematics::Vector& centreTranslation = {}) const override;

		// Rotation in quaternions
		void addRotation(const KeyFrame& keyFrame);
		void addTranslation(const KeyFrame& keyFrame);
		void addScale(const KeyFrame& keyFrame);
		void setTicksPerSecond(std::uint32_t tPerSecond);

	private:
		static mathematics::Vector interpolate(std::uint32_t timeMs, const std::vector<KeyFrame>& keyFrames, bool isRotation = false, const mathematics::Vector& defaultValue = {});
		void addKeyFrame(const KeyFrame& keyFrame, std::vector<KeyFrame>& keyFrames);
		// Three different timelines
		std::vector<KeyFrame> rotation;
		std::vector<KeyFrame> translation;
		std::vector<KeyFrame> scale;

		// Duration of the whole animation
		std::uint32_t duration;
		std::uint32_t ticksPerSecond;
	};
}

