
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
#include "config.hpp"
#include <string>


_PDX_NAMESPACE_BEGIN

/* generate_int_array< N, template<size_t> F >::result
 * - N is the number of elements in the array result::data
 * - F is a parameterized type s.t. F<I>::value will be the value of result::data[I]
 *
 * template metaprogramming technique for constructing const int arrays at compile time -- could easily be generalized
 * to any constant value type */
template<int... args> struct IntArrayHolder {
    static const int data[sizeof...(args)];
};

template<int... args>
const int IntArrayHolder<args...>::data[sizeof...(args)] = { args... };

// recursive case
template<size_t N, template<size_t> class F, int... args>
struct generate_int_array_impl {
    typedef typename generate_int_array_impl<N-1, F, F<N>::value, args...>::result result;
};

// base case
template<template<size_t> class F, int... args>
struct generate_int_array_impl<0, F, args...> {
    typedef IntArrayHolder<F<0>::value, args...> result;
};

template<size_t N, template<size_t> class F>
struct generate_int_array {
    typedef typename generate_int_array_impl<N-1, F>::result result;
};

class parser;

/* exp10<N> -- static computation of the Nth power of 10 */
template <size_t N> struct exp10    { enum { value = 10 * exp10<N-1>::value }; }; // recursive case
template <>         struct exp10<0> { enum { value = 1 }; }; // base case

/* fp_decimal */
template<uint FractionalDigits = 3>
class fp_decimal {

    static_assert(FractionalDigits > 0, "fp_decimal cannot be used as an integer (no fractional digits)");
    static_assert(9 >= FractionalDigits, "fp_decimal cannot represent more than 9 fractional digits");

    int32_t _m; // representation

public:
    static const int32_t scale = exp10<FractionalDigits>::value; // e.g., 10^3 = 1000

    /* min and max possible values for the integral component */
    static const int32_t integral_max = (INT32_MAX - scale - INT32_MAX % scale) / scale;
    static const int32_t integral_min = (INT32_MIN + scale - INT32_MIN % scale) / scale;

private:

public:
    fp_decimal(const char* src, const parser* p_parser = nullptr);
    fp_decimal(double f) : _m( f * scale ) { }

    int32_t integral()   const noexcept { return _m / scale; }
    int32_t fractional() const noexcept { return _m % scale; }

    double to_double() const { return integral() + (double)fractional() / scale; }

private:
    void throw_range_error(int64_t val, const parser* p_parser) const;

    /* fractional digit power const-tbl */
    template<size_t I> struct fractional_digit_power { enum { value = exp10<FractionalDigits-I-1>::value }; };
    typedef typename generate_int_array<FractionalDigits, fractional_digit_power>::result fractional_digit_powers;
};


_PDX_NAMESPACE_END


#include "parser.hpp"
#include "error.hpp"
#include <cstring>
#include <cstdlib>
#include <iomanip>

template<uint D>
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
fp_decimal<D>::fp_decimal(const char* src, const parser* p_parser) {

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

        int power = 1;

        for (const char* p = s_radix_pt - 1; p >= s_i; --p) {
            int d = *p - (int)'0';
            assert(0 <= d && d <= 9); // guaranteed by DECIMAL token
            i += power * d;
            power *= 10;
        }
    }

    /* enforce integral bounds */
    // FIXME: this too should be a non-critical error (and thus not an exception)
    if (i < integral_min || i > integral_max)
        throw_range_error(i, p_parser);

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


        if (false && *p) {
            /* emit some kind of non-critical error (not an exception) to the parser so that it can warn about
             * data truncation due to insufficient fractional digits */
        }
    }

    /* done */
    _m = (is_negative) ? i * -scale - f
                       : i * scale + f;
}


template<uint D>
void fp_decimal<D>::throw_range_error(int64_t val, const parser* p_parser) const {
    if (p_parser != nullptr)
        throw va_error("Integral value %ld is too big (supported range: [%d, %d]) in decimal number at %s:L%d",
                       val, integral_min, integral_max, p_parser->pathname(), p_parser->line());
    else
        throw va_error("Integral value %ld is too big (supported range: [%d, %d]) in decimal number",
                       val, integral_min, integral_max);
}


_PDX_NAMESPACE_END
