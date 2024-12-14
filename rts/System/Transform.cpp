#include "Transform.hpp"

#include "System/SpringMath.h"

CR_BIND(Transform, )
CR_REG_METADATA(Transform, (
	CR_MEMBER(r),
	CR_MEMBER(t),
	CR_MEMBER(s)
))

static_assert(sizeof(Transform) == 3 * 4 * sizeof(float));
static_assert(alignof(Transform) == alignof(decltype(Transform::r)));

Transform Transform::FromMatrix(const CMatrix44f& mat)
{
	Transform tra;
	std::tie(tra.t, tra.r, tra.s) = CQuaternion::DecomposeIntoTRS(mat);
#ifdef _DEBUG
	const float3 v{ 100, 200, 300 };
	auto vMat = mat * v;
	auto vTra = tra * v;

	auto vMatN = vMat; vMatN.Normalize();
	auto vTraN = vTra; vTraN.Normalize();

	assert(math::fabs(1.0f - vMatN.dot(vTraN)) < 0.05f);
#endif
	return tra;
}

CMatrix44f Transform::ToMatrix() const
{
	// M = T * R * S;
	/*
	(r0 * sx, r1 * sx, r2 * sx, 0)
	(r3 * sy, r4 * sy, r5 * sy, 0)
	(r6 * sz, r7 * sz, r8 * sz, 0)
	(tx     , ty     , tz     , 1)
	*/

	// therefore
	CMatrix44f m = r.ToRotMatrix();
	m.Scale(s);
	m.SetPos(t); // m.Translate() will be wrong here

	CMatrix44f ms; ms.Scale(s);
	CMatrix44f mr = r.ToRotMatrix();
	CMatrix44f mt; mt.Translate(t);

	CMatrix44f m2 = mt * mr * ms;

	//assert(m == m2);
#ifdef _DEBUG
	//auto [t_, r_, s_] = CQuaternion::DecomposeIntoTRS(m);

	const float3 v{ 100, 200, 300 };
	auto vMat = m * v;
	auto vTra = (*this) * v;

	auto vMatN = vMat; vMatN.Normalize();
	auto vTraN = vTra; vTraN.Normalize();

	assert(math::fabs(1.0f - vMatN.dot(vTraN)) < 0.05f);
#endif
	return m;
}

Transform Transform::InvertAffine() const
{
	// TODO check correctness
	const auto invR = r.Inverse();
	const auto invS = float4{ 1.0f / s.x, 1.0f / s.y, 1.0f / s.z, 0.0f };
	return Transform{
		invR,
		invR.Rotate(-t * invS),
		invS,
	};
}

bool Transform::equals(const Transform& tra) const
{
	return
		r.equals(tra.r) &&
		t.equals(tra.t) &&
		s.equals(tra.s);
}

Transform Transform::operator*(const Transform& childTra) const
{
	// TODO check correctness

	return Transform{
		r * childTra.r,
		t + r.Rotate(s * childTra.t),
		s * childTra.s
	};
}

float3 Transform::operator*(const float3& v) const
{
	// Scale, Rotate, Translate
	// same order as CMatrix44f's vTra = T * R * S * v;
	return r.Rotate(v * s) + t;
}

float4 Transform::operator*(const float4& v) const
{
	// same as above
	return r.Rotate(v * s) + t;
}
