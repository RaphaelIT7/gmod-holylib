#ifndef _IVP_LINEAR
#define _IVP_LINEAR

#include "ivp_types.h"

class IVP_U_Straight;
class IVP_U_Hesse;
class IVP_U_Quat;
class IVP_U_Point;
class IVP_U_Float_Quat;

enum IVP_COORDINATE_INDEX {
    IVP_INDEX_X = 0,  // you can rely on these
    IVP_INDEX_Y = 1,  // numbers - they will
    IVP_INDEX_Z = 2   // never be changed.
};

inline void ivp_byte_swap4(uint& fourbytes)
{
#ifdef _WIN32
	fourbytes = _byteswap_ulong(fourbytes);
#else
	struct FOURBYTES {
		union { 
			unsigned char b[4];
			uint v;
		};
	} in, out;

	in.v = fourbytes;

	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	fourbytes = out.v;
#endif
}

class IVP_U_Float_Point {
public:
    IVP_FLOAT k[3];
#ifdef IVP_VECTOR_UNIT_FLOAT
    union {
	IVP_FLOAT hesse_val;
    };
#endif

#if !defined(IVP_NO_DOUBLE)
    inline void set(const IVP_U_Point *p);
#endif
    inline void set(const IVP_FLOAT p[3]);
    //inline void set(const IVP_U_Float_Point *p);
    inline void set(IVP_FLOAT k0, IVP_FLOAT k1, IVP_FLOAT k2);			

    inline void set_negative(const IVP_U_Float_Point *p_source);	// this = p_source * -1.0
    inline void set_to_zero();					// i.e. set(0.0, 0.0, 0.0)
    
    inline IVP_DOUBLE quad_length()const;				// length of vector square: k[0]*k[0] + k[1]*k[1] + k[2]*k[2]

    inline void mult(IVP_DOUBLE factor);				// this *= factor

    inline void add(const IVP_U_Float_Point *v2); 				// this += v2
    inline void add(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2);	// this = v1 + v2;

    inline void add_multiple(const IVP_U_Float_Point *v, IVP_DOUBLE factor);	// this += v1 * factor
    inline void add_multiple(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2, IVP_DOUBLE factor2);
    
    inline void set_multiple(const IVP_U_Quat *q_source,IVP_DOUBLE f);		// this = q_source.xyz * f
#if !defined(IVP_NO_DOUBLE)
    inline void set_multiple(const IVP_U_Point *v,IVP_DOUBLE f);		// this = q_source.xyz * f
#endif
    inline void set_multiple(const IVP_U_Float_Point *v,IVP_DOUBLE f);		// this = q_source.xyz * f
    inline void set_pairwise_mult(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2);    // this->k[i] = v1->k[i] * v2->k[i]

#if !defined(IVP_NO_DOUBLE)
    inline void subtract(const IVP_U_Point *v1, const IVP_U_Point *v2);		// this = v1 - v2
#endif
    inline void subtract(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2);		// this = v1 - v2
    inline void subtract(const IVP_U_Float_Point *v2);				// this = this - v2;

    inline void inline_subtract_and_mult(const IVP_U_Float_Point *v1,const IVP_U_Float_Point *v2, IVP_DOUBLE factor); // this = (v1-v2) * factor
#if !defined(IVP_NO_DOUBLE)
    inline void inline_subtract_and_mult(const IVP_U_Point *v1,const IVP_U_Point *v2, IVP_DOUBLE factor); // this = (v1-v2) * factor
#endif 
    inline IVP_DOUBLE dot_product(const IVP_U_Float_Point *v2) const;

    inline void inline_calc_cross_product(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2); // this = v1 x v2
    inline void inline_calc_cross_product_and_normize(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2); // this = v1 x v2

    inline void inline_set_vert_to_area_defined_by_three_points(const IVP_U_Float_Point *tp0,const IVP_U_Float_Point *tp1,const IVP_U_Float_Point *tp2);
    void calc_cross_product(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2); // this = v1 x v2

    void line_sqrt(); // Sqrt of each element: k[i] = sqrt(k[i])    
    
    void set_interpolate(const IVP_U_Float_Point *p0,const IVP_U_Float_Point *p1, IVP_DOUBLE s);
    void set_orthogonal_part(const IVP_U_Float_Point *vector,const IVP_U_Float_Point *normal_v);   //project vector on surface given by normal_v

    void rotate(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle);   // rotate the point around origin

    
    IVP_DOUBLE real_length()const;		// real length = sqrt(quad_length)
    IVP_DOUBLE fast_real_length()const;	// real length with 0.1f% error, really fast
    IVP_DOUBLE real_length_plus_normize();	// Normize vector and return former real length.
    inline IVP_DOUBLE quad_distance_to(const IVP_U_Float_Point *p)const; // quad distance from this point to another

    IVP_RETURN_TYPE normize();		// normize vector (-> real_length == 1.0f)
    IVP_RETURN_TYPE fast_normize();	// normize vector (0.1f% error)
    void print(const char *comment = 0) const;

    IVP_U_Float_Point() = default;
    IVP_U_Float_Point(IVP_DOUBLE x, IVP_DOUBLE y,IVP_DOUBLE z){ k[0] = (IVP_FLOAT)x; k[1] = (IVP_FLOAT)y; k[2] = (IVP_FLOAT)z;};
    IVP_U_Float_Point(const IVP_U_Float_Point *p);

#if !defined(IVP_NO_DOUBLE)
    IVP_U_Float_Point(const IVP_U_Point *p);
#endif

	inline void byte_swap() {   ivp_byte_swap4( (uint&) k[0] );
								ivp_byte_swap4( (uint&) k[1] ); 
								ivp_byte_swap4( (uint&) k[2] ); 
						#ifdef IVP_VECTOR_UNIT_FLOAT
								ivp_byte_swap4( (uint&) hesse_val ); 
						#endif
							}

} IVP_ALIGN_16;

class IVP_U_Float_Hesse: public IVP_U_Float_Point {
public:
#if !defined(IVP_VECTOR_UNIT_FLOAT)
    IVP_FLOAT hesse_val;
#endif
    //void set4(const IVP_U_Float_Hesse *h){ this->set(h); this->hesse_val = h->hesse_val;};
    void calc_hesse(const IVP_U_Float_Point *p0,const IVP_U_Float_Point *p1,const IVP_U_Float_Point *p2);
    void calc_hesse_val(const IVP_U_Float_Point *p0);
    void proj_on_plane(const IVP_U_Float_Point *p, IVP_U_Float_Point *result) const;	// sets result to a point on the plane and nearest to p
    void mult_hesse(IVP_DOUBLE factor);						// scale the hesse
    void normize();								// normize hesse (->super.calc_quad_length() == 1.0f)
    // get the distance between a point and the plane
    inline IVP_DOUBLE get_dist(const IVP_U_Float_Point *p) const;				// get the distance between a point and the plane

    IVP_U_Float_Hesse() = default;
    IVP_U_Float_Hesse(IVP_DOUBLE xi, IVP_DOUBLE yi, IVP_DOUBLE zi, IVP_DOUBLE val) { k[0]=(IVP_FLOAT)xi; k[1]=(IVP_FLOAT)yi; k[2]=(IVP_FLOAT)zi; hesse_val=val; }

	void byte_swap() { ivp_byte_swap4( (uint&) hesse_val ); IVP_U_Float_Point::byte_swap(); }
};

#if defined(IVP_NO_DOUBLE)
class IVP_U_Point: public IVP_U_Float_Point {
public:

#else
class IVP_U_Point {
public:
    IVP_DOUBLE k[3];
#	ifdef IVP_VECTOR_UNIT_DOUBLE    
    union {
	IVP_DOUBLE hesse_val;
    };
#	endif    
    inline IVP_DOUBLE dot_product(const IVP_U_Point *v2) const   {    return( k[0]*v2->k[0] + k[1]*v2->k[1] + k[2]*v2->k[2] );   };
    inline IVP_DOUBLE dot_product(const IVP_U_Float_Point *v2) const   {    return( k[0]*v2->k[0] + k[1]*v2->k[1] + k[2]*v2->k[2] );   };
    
    inline void inline_calc_cross_product(const IVP_U_Point *v1, const IVP_U_Point *v2); // this = v1 x v2
    inline void inline_calc_cross_product_and_normize(const IVP_U_Point *v1, const IVP_U_Point *v2); // this = v1 x v2
    void calc_cross_product(const IVP_U_Point *v1, const IVP_U_Point *v2); // this = v1 x v2

    inline void set_to_zero();							// == set(0.0, 0.0, 0.0)
    inline void set(const IVP_U_Point *p_source);				// this = p_source
    inline void set(const IVP_U_Float_Point *p_source);				// this = (IVP_DOUBLE)p_source
    inline void set(const IVP_U_Quat *q_source);				// this = q_source.xyz
    inline void set(IVP_DOUBLE k0, IVP_DOUBLE k1, IVP_DOUBLE k2 = 0.0f);			
    inline void set(const IVP_FLOAT p[3]);			    


    inline IVP_DOUBLE quad_length()const;	 	// Square length of vector: k[0]*k[0] + k[1]*k[1] + k[2]*k[2].
    inline IVP_DOUBLE quad_distance_to(const IVP_U_Point *p)const; // Square distance from this point to another
    inline IVP_DOUBLE quad_distance_to(const IVP_U_Float_Point *p)const; // Square distance from this point to another.
    
    IVP_DOUBLE real_length()const;		// Returns real length (= sqrt(quad_length)).
    IVP_DOUBLE real_length_plus_normize();	// Normize vector and return former real length.
    IVP_DOUBLE fast_real_length()const;	// Returns real length with max. 0.1f% error, really fast if in cache.
    IVP_RETURN_TYPE normize();		// Normize vector (-> real length == 1.0f), return IVP_FALSE if length too small.
    IVP_RETURN_TYPE fast_normize();	// Normize vector (0.1f% error)
    
    

    inline void add(const IVP_U_Point *v2); 					// this = this + v2;
    inline void add(const IVP_U_Float_Point *v2); 				// this = this + v2;
    inline void add(const IVP_U_Point *v1, const IVP_U_Point *v2);	// this = v1 + v2;
    inline void add(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2);	// this = v1 + v2;
    inline void add_multiple(const IVP_U_Point *v, IVP_DOUBLE factor);		// this += v1 * factor
    inline void add_multiple(const IVP_U_Float_Point *v, IVP_DOUBLE factor);	// this += v1 * factor
    inline void add_multiple(const IVP_U_Point *v1, const IVP_U_Point *v2, IVP_DOUBLE factor2); // this = v1 + factor2*v2  .
    inline void add_multiple(const IVP_U_Point *v1, const IVP_U_Float_Point *v2, IVP_DOUBLE factor2); // this = v1 + factor2*v2  .

    
    inline void subtract(const IVP_U_Point *v2); 				// this = this - v2;
    inline void subtract(const IVP_U_Float_Point *v2); 				// this = this - v2;
    inline void subtract(const IVP_U_Point *v1,const IVP_U_Point *v2); 		// this = v1 - v2;
    inline void subtract(const IVP_U_Float_Point *v1, const IVP_U_Float_Point *v2); // this = v1 - v2;
    inline void subtract(const IVP_U_Float_Point *v1, const IVP_U_Point *v2); // this = v1 - v2;
    inline void subtract(const IVP_U_Point *v1, const IVP_U_Float_Point *v2); // this = v1 - v2;
    inline void inline_subtract_and_mult(const IVP_U_Point *v1,const IVP_U_Point *v2, IVP_DOUBLE factor); // this = (v1-v2) * factor
    inline void inline_subtract_and_mult(const IVP_U_Float_Point *v1,const IVP_U_Float_Point *v2, IVP_DOUBLE factor); // this = (v1-v2) * factor
    inline void set_negative(const IVP_U_Point *p_source);			// this = p_source * -1.0

    inline void mult(IVP_DOUBLE factor);						// this *= factor
    inline void set_multiple(const IVP_U_Point *p,IVP_DOUBLE f);		// this = p * f
    inline void set_multiple(const IVP_U_Float_Point *p,IVP_DOUBLE f);	// this = p * f
    inline void set_multiple(const IVP_U_Quat *q_source,IVP_DOUBLE f);		// this = q_source.xyz * f
    inline void set_pairwise_mult(const IVP_U_Point *v1, const IVP_U_Point *v2);    // this->k[i] = v1->k[i] * v2->k[i]
    inline void set_pairwise_mult(const IVP_U_Point *v1, const IVP_U_Float_Point *v2);    // this->k[i] = v1->k[i] * v2->k[i]
    

    inline void inline_set_vert_to_area_defined_by_three_points(const IVP_U_Point *tp0,const IVP_U_Point *tp1,const IVP_U_Point *tp2);
    inline void inline_set_vert_to_area_defined_by_three_points(const IVP_U_Float_Point *tp0,const IVP_U_Float_Point *tp1,const IVP_U_Point *tp2);
    inline void inline_set_vert_to_area_defined_by_three_points(const IVP_U_Float_Point *tp0,const IVP_U_Float_Point *tp1,const IVP_U_Float_Point *tp2);

    void set_interpolate(const IVP_U_Point *p0,const IVP_U_Point *p1, IVP_DOUBLE s);	/* linear interpolation between p0 and p1:
											 * this = p1 * s + p0 * (1.0f-s) */
    void set_interpolate(const IVP_U_Float_Point *p0,const IVP_U_Float_Point *p1, IVP_DOUBLE s);	/* linear interpolation between p0 and p1:
												 * this = p1 * s + p0 * (1.0f-s) */
#endif
    
    /***** The following function change 'this' ****/ 
    // INTERN_START
    IVP_BOOL is_parallel(const IVP_U_Point *v2, IVP_DOUBLE eps) const; // checks length of crossproduct against eps
    
    void calc_an_orthogonal(const IVP_U_Point *ip);
    
    void solve_quadratic_equation_fast(const IVP_U_Point *a);	/* Solve a->k[0]*x*x + a->k[1]*x + a->k[2] = 0,
								 * this->k[0] = term under squareroot, this->k[1] = smaller value
								 * this->k[2] = greater value,
								 * if this->k[0] < 0: no result.
								 * Uses fast squareroot */
    void solve_quadratic_equation_accurate(const IVP_U_Point *a);	/* Solve a->k[0]*x*x + a->k[1]*x + a->k[2] = 0,
									 * this->k[0] = term under squareroot, this->k[1] = smaller value
									 * this->k[2] = greater value,
									 * if this->k[0] < 0: no result. */

    // INTERN_END


    void set_orthogonal_part(const IVP_U_Point *vector,const IVP_U_Point *normal_v); //project vector on surface given by normal_v
    void set_orthogonal_part(const IVP_U_Point *vector,const IVP_U_Float_Point *normal_v); //project vector on surface given by normal_v
    IVP_RETURN_TYPE set_crossing(IVP_U_Hesse *h0, IVP_U_Hesse *h1, IVP_U_Hesse *h2); // set this to the crossing of three areas

    void rotate(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle);   // rotate the point around origin
    
    void line_min(const IVP_U_Point *p); // sets this to linewise min
    void line_max(const IVP_U_Point *p); // sets this to linewise max
    
    void print(const char *comment = 0);

    IVP_U_Point() = default;
    inline IVP_U_Point(const IVP_U_Float_Point &p);
    IVP_U_Point(IVP_DOUBLE x, IVP_DOUBLE y,IVP_DOUBLE z){ k[0] = x; k[1] = y; k[2] = z;};

	//inline void byte_swap() { IVP_ASSERT( 0 && "No byte swap for doubles yet"); CORE; }

} IVP_ALIGN_16;

#endif