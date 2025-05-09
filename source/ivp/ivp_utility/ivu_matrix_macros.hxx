#ifndef IVU_MATRIX_MACROS_H
#define IVU_MATRIX_MACROS_H

void IVP_U_Matrix3::inline_mimult3(const IVP_U_Matrix3 *mb, IVP_U_Matrix3 *m_out) const
{
    IVP_U_Point col[3];
    this->get_col( IVP_INDEX_X, &col[0]);
    this->get_col( IVP_INDEX_Y, &col[1]);
    this->get_col( IVP_INDEX_Z, &col[2]);
    for (int c = 0;c<3;c++){
	mb->inline_vimult3(&col[c], &col[c] );
    }
    m_out->rows[0].set(&col[0]);
    m_out->rows[1].set(&col[1]);
    m_out->rows[2].set(&col[2]);
}


void IVP_U_Matrix3::inline_mmult3(const IVP_U_Matrix3 *mb, IVP_U_Matrix3 *m_out) const {
    IVP_U_Point row[3];
    for (int r = 0;r<3;r++){
	mb->inline_vimult3(&rows[r], &row[r] );
    }
    m_out->rows[0].set(&row[0]);
    m_out->rows[1].set(&row[1]);
    m_out->rows[2].set(&row[2]);
}

void IVP_U_Matrix::inline_mmult4(const IVP_U_Matrix *mb, IVP_U_Matrix *m_out) const
{
	IVP_U_Point v_shift_korr;
	this->vmult3(&mb->vv,&v_shift_korr);
	inline_mmult3(mb,m_out);
	m_out->vv.add(&this->vv, &v_shift_korr);
}

void IVP_U_Matrix::inline_mimult4(const IVP_U_Matrix *mb, IVP_U_Matrix *m_out) const
{
	IVP_U_Point v_shift_korr;
	v_shift_korr.subtract(&mb->vv, &this->vv);
	this->vimult3(&v_shift_korr,&m_out->vv);
	this->inline_mimult3(mb,m_out);
}


void IVP_U_Matrix::get_4x4_column_major( class hk_Transform *o ) const
{
	static_assert( sizeof(*o) >= sizeof(hk_real) * 16 );

	reinterpret_cast<hk_real*>(o)[0] = get_elem( 0, 0 );
	reinterpret_cast<hk_real*>(o)[1] = get_elem( 1, 0 );
	reinterpret_cast<hk_real*>(o)[2] = get_elem( 2, 0 );
	reinterpret_cast<hk_real*>(o)[3] = 0.0f;

	reinterpret_cast<hk_real*>(o)[4] = get_elem( 0, 1 );
	reinterpret_cast<hk_real*>(o)[5] = get_elem( 1, 1 );
	reinterpret_cast<hk_real*>(o)[6] = get_elem( 2, 1 );
	reinterpret_cast<hk_real*>(o)[7] = 0.0f;

	reinterpret_cast<hk_real*>(o)[8] =	get_elem( 0, 2 );
	reinterpret_cast<hk_real*>(o)[9] =	get_elem( 1, 2 );
	reinterpret_cast<hk_real*>(o)[10] = get_elem( 2, 2 );
	reinterpret_cast<hk_real*>(o)[11] = 0.0f;

	reinterpret_cast<hk_real*>(o)[12] =	vv.k[0];
	reinterpret_cast<hk_real*>(o)[13] =	vv.k[1];
	reinterpret_cast<hk_real*>(o)[14] = vv.k[2];
	reinterpret_cast<hk_real*>(o)[15] = 1.0f;
}


IVP_DOUBLE IVP_U_Quat::inline_estimate_q_diff_to(const IVP_U_Float_Quat *to) const {	// roughly estimate the quad alpha
    // cosinus of half angle
    IVP_DOUBLE cosom2 = this->x * to->x + this->y * to->y + this->z * to->z + this->w * to->w;
    IVP_DOUBLE qcosom2 = cosom2 * cosom2;
    IVP_DOUBLE qsin2 = 1 - qcosom2;		// quad sin
    return qsin2 * 2;
}

#endif
