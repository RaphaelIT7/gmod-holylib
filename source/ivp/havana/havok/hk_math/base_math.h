#ifndef HK_MATH_MATH_H
#define HK_MATH_MATH_H

#include <hk_base/base.h>
#include <hk_math/types.h>

#include <cmath>

constexpr inline hk_real HK_PI{3.14159265358979323846f};   /* pi */
constexpr inline hk_real HK_PI_2{1.57079632679489661923f}; /* pi/2 */

constexpr inline hk_real HK_REAL_MAX{1e16f};
constexpr inline hk_real HK_REAL_EPS{1e-16f}; /* the minumum resolution of real */
constexpr inline hk_real HK_REAL_RES{1e-7f};  /* resolution of hk_real of  relative to 1.0f */
constexpr inline hk_double HK_DOUBLE_RES{1E-12};	// double resolution for numbers < 1.0


class hk_Math
{
	public:
		static inline hk_double fabsd[[nodiscard]] ( hk_double );

		static inline hk_real sqrt [[nodiscard]] ( hk_real );
		static inline hk_real sqrt_inv [[nodiscard]] ( hk_real );
		static inline hk_real fast_sqrt [[nodiscard]] ( hk_real );
		static inline hk_real fast_sqrt_inv [[nodiscard]] ( hk_real );

		static inline hk_real fabs [[nodiscard]] (hk_real);
		static inline hk_real tan [[nodiscard]] ( hk_real );
		static inline hk_real sin [[nodiscard]] ( hk_real );
		static inline hk_real cos [[nodiscard]] ( hk_real );
		static inline hk_real atan2 [[nodiscard]] ( hk_real sinus, hk_real cosinus);
		static inline hk_real fast_approx_atan2 [[nodiscard]] ( hk_real sinus, hk_real cosinus); // 0.01 error
		static inline hk_real fast_approx_atan2_normized [[nodiscard]] ( hk_real sinus, hk_real cosinus); // 0.01 error // input has to be normized
		
		static inline hk_real asin [[nodiscard]] (hk_real);
		static inline hk_real acos [[nodiscard]] ( hk_real );
		static inline hk_real max [[nodiscard]] ( hk_real, hk_real );
		static inline hk_real min [[nodiscard]] ( hk_real, hk_real );

		static inline hk_real floor [[nodiscard]] ( hk_real );
		static inline hk_real ceil [[nodiscard]] ( hk_real );
		static inline hk_real clamp [[nodiscard]] ( hk_real v, hk_real min, hk_real max );
		static inline hk_real pow [[nodiscard]] ( hk_real, hk_real );
		static inline hk_real exp [[nodiscard]] ( hk_real );

		static void srand01( unsigned seedVal );
		static inline hk_real _rand01 [[nodiscard]] ();
		static	      hk_real rand01 [[nodiscard]] ();
		static inline int int_log2 [[nodiscard]] ( hk_real ); // integer part of log2
		static unsigned int hk_random_seed;

		static bool almost_equal [[nodiscard]] ( hk_real a, hk_real b, hk_real eps = HK_REAL_RES );
		static bool almost_equal [[nodiscard]] ( hk_double a, hk_double b, hk_double eps = HK_DOUBLE_RES );
		static bool almost_zero [[nodiscard]] ( hk_real a, hk_real eps = HK_REAL_RES );
		static bool almost_zero [[nodiscard]] ( hk_double a, hk_double eps = HK_DOUBLE_RES );
};

#ifndef IVP_NO_MATH_INL
#include <hk_math/math.inl>
#endif

#endif /* HK_MATH_MATH_H */
