#include "TransformComponents.h"
#include "Utils.h"

using candela::mathematics::Matrix;
using candela::mathematics::Vector;
using candela::mathematics::TransformComponents;

Matrix TransformComponents::transform(const Vector& origin, bool translationAbsolute) const
{
	return DirectX::XMMatrixTranslationFromVector(DirectX::XMVectorNegate(origin))
		* DirectX::XMMatrixScalingFromVector(Scale)
		* DirectX::XMMatrixRotationX(Rotate.m128_f32[0])
		* DirectX::XMMatrixRotationY(Rotate.m128_f32[1])
		* DirectX::XMMatrixRotationZ(Rotate.m128_f32[2])
		* DirectX::XMMatrixTranslationFromVector(translationAbsolute ? Translate : DirectX::XMVectorAdd(origin, Translate));
}

void TransformComponents::setFromMatrix(const Matrix& matrix)
{
	DirectX::XMMatrixDecompose(&Scale, &Rotate, &Translate, matrix);
	const auto& rot = QuaternionToRotationXYZ(Rotate);
	Rotate = DirectX::XMLoadFloat3(&rot);
}
