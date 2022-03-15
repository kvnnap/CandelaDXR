#pragma once

#include <DirectXMath.h>

namespace candela::renderer
{
	class Camera {
	public:
		Camera(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& direction, float nearWidth, float nearHeight, float nearZ, float farZ);

		// Methods
		void lookTo(const DirectX::XMVECTOR& direction, const DirectX::XMVECTOR& up = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f));

		// This method will vary the width of the sensor and keep its height intact
		void setAspectRatio(float aspectRatio);
		void incrementPosition(const DirectX::XMVECTOR& deltaPosition);
		void incrementPosition(float dx, float dy, float dz);
		void incrementPositionAlongDirection(float dx, float dy);

		void incrementDirection(float rotationY, float rotationZ);

		const DirectX::XMVECTOR& getPosition() const;
		const DirectX::XMVECTOR& getDirection() const;
		const DirectX::XMVECTOR& getUp() const;
		const DirectX::XMMATRIX& getViewMatrix() const;
		const DirectX::XMMATRIX& getPerspectiveMatrix() const;
		DirectX::XMVECTOR getNearPlaneDimensions() const;
		DirectX::XMMATRIX getViewPerspectiveMatrix() const;
		DirectX::XMMATRIX getViewPerspectiveMatrixColMajor() const;

		bool hasChanged() const;
		void resetChanged();

	private:
		void recalculateViewMatrix();
		DirectX::XMVECTOR getCrossVector() const;

		// Camera vectors
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR up;
		float nearWidth;
		float nearHeight;
		float nearZ;
		float farZ;

		DirectX::XMMATRIX viewMatrix;
		DirectX::XMMATRIX perspectiveMatrix;

		bool changed;
	};
}