#pragma once

#include <DirectXMath.h>

class DirectXVectorWrapper {
	const DirectX::XMVECTOR* vector;

public:
	DirectXVectorWrapper(const DirectX::XMVECTOR& p_vector);

	float getX() const;
	float getY() const;
	float getZ() const;
	float getW() const;
};