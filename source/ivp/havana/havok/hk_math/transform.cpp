#include <hk_math/vecmath.h>

hk_Transform::hk_Transform(const hk_Quaternion &q, const hk_Vector3 &t)
	: m_translation{t}
{
	this->set(q);
}

void hk_Transform::set_transform(const hk_Quaternion &q, const hk_Vector3 &t)
{
	this->set(q);
	m_translation = t;
}

void hk_Transform::set_cols4(	const hk_Vector3& c0,
								const hk_Vector3& c1,
								const hk_Vector3& c2,
								const hk_Vector3& c3)
{
	get_column(0) = c0;
	get_column(1) = c1;
	get_column(2) = c2;
	m_translation = c3;
}

void hk_Transform::set_identity_transform()
{
	get_column(0).set(1,0,0);
	get_column(1).set(0,1,0);
	get_column(2).set(0,0,1);
	m_translation.set(0,0,0);
}


void hk_Transform::get_4x4_column_major(hk_Transform* p) const
{
	static_assert( sizeof(*p) >= sizeof(hk_real) * 16 );
	hkString::memcpy(p, &get_column(0), sizeof(*p));

	reinterpret_cast<hk_real*>(p)[3]  = 0;
	reinterpret_cast<hk_real*>(p)[7]  = 0;
	reinterpret_cast<hk_real*>(p)[11] = 0;
	reinterpret_cast<hk_real*>(p)[15] = 1;
}


void hk_Transform::set_interpolate( const hk_QTransform &a, const hk_QTransform &b , hk_real t)
{
	_set_interpolate(a,b,t);
}
