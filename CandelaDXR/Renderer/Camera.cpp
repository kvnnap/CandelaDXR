#include "Camera.h"

using candela::renderer::Camera;

using DirectX::XMVECTOR;
using DirectX::XMMATRIX;
using DirectX::XMVector4Transform;
using DirectX::XMVector3Normalize;
using DirectX::XMVector3Cross;
using DirectX::XMMatrixRotationAxis;
using DirectX::XMMatrixLookToLH;
using DirectX::XMVectorSet;
using DirectX::operator+=;
using DirectX::operator+;
using DirectX::operator*;

Camera::Camera(const XMVECTOR& position, const XMVECTOR& direction, float nearWidth, float nearHeight, float nearZ, float farZ)
	: position(position), direction(XMVector3Normalize(direction)), up(), nearWidth(nearWidth), nearHeight(nearHeight), nearZ(nearZ), farZ(farZ), viewMatrix()
{
	lookTo(direction);
	perspectiveMatrix = DirectX::XMMatrixPerspectiveLH(nearWidth, nearHeight, nearZ, farZ);
}

void Camera::recalculateViewMatrix()
{
	viewMatrix = XMMatrixLookToLH(position, direction, XMVectorSet(0.f, 1.f, 0.f, 0.f));
}

XMVECTOR Camera::getCrossVector() const
{
	return XMVector3Cross(up, direction);
}

void Camera::lookTo(const DirectX::XMVECTOR& p_direction, const DirectX::XMVECTOR& p_up)
{
	up = XMVector3Normalize(p_up);
	direction = XMVector3Normalize(p_direction);
	XMVECTOR side = getCrossVector();
	up = XMVector3Cross(direction, side);
	recalculateViewMatrix();
}

void Camera::incrementPosition(const XMVECTOR& deltaPosition)
{
	position += deltaPosition;
	recalculateViewMatrix();
}

void Camera::incrementPosition(float dx, float dy, float dz)
{
	position += DirectX::XMVectorSet(dx, dy, dz, 0.f);
	recalculateViewMatrix();
}

void Camera::incrementPositionAlongDirection(float moveLeftRight, float moveUpDown)
{
	XMVECTOR xAxisVector = getCrossVector();
	position += moveLeftRight * xAxisVector + moveUpDown * direction;
	recalculateViewMatrix();
}

void Camera::incrementDirection(float rotationLeftRight, float rotationUpDown)
{
	XMVECTOR xAxisVector = getCrossVector();
	// XMMatrixRotationY(rotationY) * XMMatrixRotationX(rotationX) <- incorrect rotation
	// Try also XMMatrixRotationNormal, should be faster since up and direction are normalised (but may lose precision over time :/)
	XMMATRIX rotMat = XMMatrixRotationAxis(up, rotationLeftRight) * XMMatrixRotationAxis(xAxisVector, rotationUpDown);
	direction = XMVector4Transform(direction, rotMat);
	up = XMVector4Transform(up, rotMat);
	recalculateViewMatrix();
}

const XMVECTOR& Camera::getPosition() const
{
	return position;
}

const XMVECTOR& Camera::getDirection() const
{
	return direction;
}

const XMMATRIX& Camera::getViewMatrix() const
{
	return viewMatrix;
}

const XMMATRIX& Camera::getPerspectiveMatrix() const
{
	return perspectiveMatrix;
}

XMMATRIX Camera::getViewPerspectiveMatrix() const
{
	return viewMatrix * perspectiveMatrix;
}
