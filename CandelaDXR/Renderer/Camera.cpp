#include "Camera.h"

using candela::renderer::Camera;

using DirectX::XMVECTOR;
using DirectX::XMMATRIX;
using DirectX::XMVector4Transform;
using DirectX::XMVector3Normalize;
using DirectX::XMVector3Cross;
using DirectX::XMMatrixRotationAxis;
using DirectX::XMMatrixLookToRH;
using DirectX::XMMatrixTranspose;
using DirectX::XMVectorSet;
using DirectX::XMMatrixPerspectiveRH;
using DirectX::operator+=;
using DirectX::operator+;
using DirectX::operator*;

Camera::Camera(const XMVECTOR& position, const XMVECTOR& direction, float nearWidth, float nearHeight, float nearZ, float farZ, const DirectX::XMVECTOR& up)
	: position(position), direction(XMVector3Normalize(direction)), up(up), nearWidth(nearWidth), nearHeight(nearHeight), nearZ(nearZ), farZ(farZ), viewMatrix(), changed()
{
	lookTo(direction, up);
	perspectiveMatrix = XMMatrixPerspectiveRH(nearWidth, nearHeight, nearZ, farZ);
	origPosition = position;
	origDirection = this->direction;
}

void Camera::recalculateViewMatrix()
{
	viewMatrix = XMMatrixLookToRH(position, direction, up);
	changed = true;
}

XMVECTOR Camera::getCrossVector() const
{
	return XMVector3Normalize(XMVector3Cross(direction, up));
}

void Camera::lookTo(const XMVECTOR& p_direction, const XMVECTOR& p_up)
{
	up = XMVector3Normalize(p_up);
	direction = XMVector3Normalize(p_direction);
	recalculateViewMatrix();
}

void Camera::lookTo(const XMVECTOR& p_pos, const XMVECTOR& p_dir, const XMVECTOR& up)
{
	position = p_pos;
	lookTo(p_dir, up);
}

void Camera::setAspectRatio(float aspectRatio)
{
	nearWidth = aspectRatio * nearHeight;
	perspectiveMatrix = XMMatrixPerspectiveRH(nearWidth, nearHeight, nearZ, farZ);
}

void Camera::setPosition(const XMVECTOR& p_pos)
{
	position = p_pos;
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
	XMMATRIX rotMat = XMMatrixRotationAxis(up, rotationLeftRight) * XMMatrixRotationAxis(xAxisVector, rotationUpDown);
	direction = XMVector3Normalize(XMVector4Transform(direction, rotMat));
	recalculateViewMatrix();
}

void Camera::setName(const std::string& name)
{
	this->name = name;
}

const XMVECTOR& Camera::getPosition() const
{
	return position;
}

const XMVECTOR& Camera::getDirection() const
{
	return direction;
}

const XMVECTOR& Camera::getUp() const
{
	return up;
}

XMVECTOR Camera::getNearPlaneDimensions() const
{
	return XMVectorSet(nearWidth, nearHeight, nearZ, farZ);
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

XMMATRIX Camera::getViewPerspectiveMatrixColMajor() const
{
	return XMMatrixTranspose(getViewPerspectiveMatrix());
}

const std::string& Camera::getName() const
{
	return name;
}

bool Camera::hasChanged() const
{
	return changed;
}

void Camera::setChanged()
{
	changed = true;
}

void Camera::resetChanged()
{
	changed = false;
}

void Camera::transform(const mathematics::Matrix& trans)
{
	position = DirectX::XMVector3Transform(origPosition, trans);
	auto dirTrans = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, trans));
	direction = XMVector3Normalize(DirectX::XMVector4Transform(origDirection, dirTrans));
	recalculateViewMatrix();
}

const DirectX::XMVECTOR& candela::renderer::Camera::getCentrePosition() const
{
	return origPosition;
}
