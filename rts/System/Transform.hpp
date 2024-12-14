#pragma once

#include "Quaternion.h"
#include "float4.h"
#include "creg/creg_cond.h"

class CMatrix44f;

struct Transform {
	CR_DECLARE_STRUCT(Transform)
	CQuaternion r;
	float4 t;
	float4 s;

	constexpr Transform()
		: r{ CQuaternion() }
		, t{ float4() }
		, s{ float4(1.0f, 1.0f, 1.0f, 0.0f)}
	{}

	constexpr Transform(const CQuaternion& r_, const float4& t_, const float4& s_)
		: r{ r_ }
		, t{ t_ }
		, s{ s_ }
	{}

	// similar to CMatrix44f::LoadIdentity()
	void LoadIdentity() {
		r = CQuaternion();
		t = float4();
		s = float4(1.0f, 1.0f, 1.0f, 0.0f);
	}

	static Transform FromMatrix(const CMatrix44f& mat);
	CMatrix44f ToMatrix() const;

	// similar to CMatrix44f::InvertAffine, except with scale()
	Transform InvertAffine() const;

	bool equals(const Transform& tra) const;

	Transform operator*(const Transform& childTra) const;
	float3 operator*(const float3& v) const;
	float4 operator*(const float4& v) const;

	Transform& operator*=(const Transform& childTra) { *this = (*this) * childTra; return *this; }
};