
/* pdx::fp_decimal -- a class to represent parsed fixed-point decimal numbers accurately (i.e., fractional base-10^-N
 * component so that any parsed decimal number can be represented exactly).
 *
 * like with pdx::date, our string input constructor assumes that the number-string is well-formed.
 *
 * NOTE: currently, fp_decimal DOES NOT IMPLEMENT ARITHMETIC AT ALL, although it is prepared for that functionality.
 *       this is because native fixed point decimal arithmetic is simply not required for current use cases, and if
 *       the rare bit of arithmetic is required, conversion to/from floating-point is at least there.
 */

#pragma once
#include "pdx_common.hpp"

#include "file_location.hpp"
#include "error_queue.hpp"


_PDX_NAMESPACE_BEGIN


/* exp10<N> -- static computation of the Nth power of 10 */
template <size_t N> struct exp10    { enum { value = 10 * exp10<N-1>::value }; }; // recursive case
template <>         struct exp10<0> { enum { value = 1 }; }; // base case

/* fp_decimal */
template<uint FractionalDigits = 3>
class fp_decimal {
    typedef fp_decimal<FractionalDigits> self_t;

    static_assert(FractionalDigits > 0, "fp_decimal cannot be used as an integer (no fractional digits)");
    static_assert(9 >= FractionalDigits, "fp_decimal cannot represent more than 9 fractional digits");

    int32_t _m; // representation

public:
    static const int32_t scale = exp10<FractionalDigits>::value; // e.g., 10^3 = 1000

    /* min and max possible values for the integral component */
    static const int32_t integral_max = (INT32_MAX - scale - INT32_MAX % scale) / scale;
    static const int32_t integral_min = (INT32_MIN + scale - INT32_MIN % scale) / scale;
    static const int32_t invalid = INT32_MIN; // cannot be represented in any fp_decimal<D in 1..9>, so we'll use it as our NaN

public:
    fp_decimal(const char* src, const file_location&, error_queue&);
    fp_decimal(double f) : _m( f * scale + 0.5 )  {}
    fp_decimal(float f)  : _m( f * scale + 0.5f ) {}
    fp_decimal(int i)    : _m( i * scale ) {}

    int32_t integral()   const noexcept { return _m / scale; }
    int32_t fractional() const noexcept { return _m % scale; }

    double to_double() const noexcept { return double(_m) / scale; }
    float  to_float()  const noexcept { return float(_m) / scale; }

    bool operator< (const self_t& o) const noexcept { return _m < o._m; }
    bool operator> (const self_t& o) const noexcept { return _m > o._m; }
    bool operator<=(const self_t& o) const noexcept { return _m <= o._m; }
    bool operator>=(const self_t& o) const noexcept { return _m >= o._m; }
    bool operator==(const self_t& o) const noexcept { return _m == o._m; }
    bool operator!=(const self_t& o) const noexcept { return _m != o._m; }

    bool operator< (int i) const noexcept { return _m < i * scale; }
    bool operator> (int i) const noexcept { return _m > i * scale; }
    bool operator<=(int i) const noexcept { return _m <= i * scale; }
    bool operator>=(int i) const noexcept { return _m >= i * scale; }
    bool operator==(int i) const noexcept { return _m == i * scale; }
    bool operator!=(int i) const noexcept { return _m != i * scale; }

private:
    /* fractional digit power const-tbl */
    template<size_t I> struct fractional_digit_power { enum { value = exp10< FractionalDigits - I - 1 >::value }; };
    typedef typename generate_int_array<FractionalDigits, fractional_digit_power>::result fractional_digit_powers;
};


_PDX_NAMESPACE_END


#include <cstring>
#include <cstdlib>
#include <iomanip>


template<unsigned int D>
std::ostream& operator<<(std::ostream& os, pdx::fp_decimal<D> fp) {
    os << fp.integral();

    if (int f = abs(fp.fractional()))
        os << '.' << std::setfill('0') << std::setw(D) << f;

    return os;
}


_PDX_NAMESPACE_BEGIN


/* construct from well-formed c-string. as mentioned, this conversion routine is intended to be run by the parser after
 * lexical analysis has already guaranteed that the string is well-formed. we do not attempt to detect or handle various
 * possible types of errors or account for input format variability which would be redundant with the DECIMAL type token
 * definition, which is defined as:
 *
 * DECIMAL: -?[0-9]+\.[0-9]*
 */
template<uint D>
fp_decimal<D>::fp_decimal(const char* src, const file_location& loc, error_queue& errq) {

    bool is_negative = false;
    const char* s_i = src;

    if (*src == '-') {
        is_negative = true;
        ++s_i;
    }

    const char* s_radix_pt = strchr(src, '.');
    assert( s_radix_pt && s_radix_pt != s_i ); // guaranteed by DECIMAL token
    const char* s_f = s_radix_pt + 1;

    /* [s_i,s_radix_pt) now points to integer portion, s_f points to fractional portion (also an integer).
     *
     * interpret integral portion first as a 64-bit integer so that we may detect values which
     * cannot be represented. */

    /* parse integral component of s_i */
    int64_t i = 0;
    {
        int overflow = 0;
        int power = 1;

        for (const char* p = s_radix_pt - 1; p >= s_i; --p) {
            int d = *p - (int)'0';
            assert(0 <= d && d <= 9); // guaranteed by DECIMAL token
            i += power * d;
            power *= 10;
            overflow |= (is_negative) ? i < integral_min : i > integral_max;
        }

        if (overflow)
            errq.enqueue(loc, "Integral value is too big in decimal number -- supported range: [%d, %d]",
                         integral_min+0, integral_max+0);

    }

    /* parse fractional component s_f */
    int32_t f = 0;
    {
        /* NOTE: the approach here, though it could be coded more dynamically like the algorithm used for
         * the integral component, is an attempt to use static code generation for faster and simpler
         * execution. I may very well change it to something like the dynamic algorithm above (seek to
         * end of string, calculate length, from length calculate starting digit power-- it's always 1 above,
         * but for the fractional component, digit powers start exponentiating possibly past the end of the
         * string -- and then reverse-iterate over the digit characters, exponentiating as we go), but since
         * I bothered with this bit of magic, might as well see how it performs.
         *
         * as for why this approach itself: it should be easy for the compiler to unroll the loop, all math
         * re: exponentiation is rolled into precalculated compile-time constants, etc. if the approach
         * proves especially performant, then we can use the same one for the integral component. note that
         * we could also theoretically eliminate the inner branch and possibly a few more improvements.
         *
         * [totally academic IOW]
         */
        const char* p = s_f;

        for (uint i = 0; i < D; ++i) {
            int c = *p;
            if (!c) break;
            c -= (int)'0';
            assert(0 <= c && c <= 9); // guaranteed by DECIMAL token
            f += fractional_digit_powers::data[i] * c;
            ++p;
        }


        if (*p) {
            /* assuming *p is a digit (guaranteed by DECIMAL token), then:
             * data truncation due to insufficient fractional digits in representation */
            errq.enqueue(error::priority::WARNING, loc,
                         "Fractional value '%s' is too big in decimal number -- supported range: [0, %d]; value truncated",
                         s_f, scale - 1);
        }
    }

    /* done */
    _m = (is_negative) ? i * -scale - f
                       : i * scale + f;
}


_PDX_NAMESPACE_END
