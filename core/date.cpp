
#include "date.hpp"
#include "parser.hpp"

#include <cstring>
#include <cstdlib>

using namespace pdx;

/* construct a `date` from a well-formed date-string non-const `src` (we assume the str is thrown away, as we modify it)
 * intended to be used when `src` is already known to be well-formed due to lexical analysis, as we skip error-checking.
 */
date::date(char* src, const parser* p_lex) {
    char* part[3];
    part[0] = src;

    for (int i = 0; i < 2; ++i) {
        char* end = strchr(part[i], '.');
        part[i + 1] = end + 1;
        *end = '\0';
    }

    /* part[] now contains 3 null-terminated strings for the year, month, and day.
     * make sure our object will be able to hold the parsed values.
     * be permissive of all other problems w/ date components so that we can later report what was actually parsed.
     */

    const char* name[3] = { "year", "month", "day" };
    const uint  max[3]  = { UINT16_MAX, UINT8_MAX, UINT8_MAX };

    uint num[3]; // must be unsigned due to lexical analysis

    for (int i = 0; i < 3; ++i) {
        num[i] = (unsigned) atoi( part[i] );

        if (num[i] > max[i])
            throw_bounds_error(name[i], num[i], max[i], p_lex);
    }

    _y = static_cast<uint16_t>( num[0] );
    _m = static_cast<uint8_t> ( num[1] );
    _d = static_cast<uint8_t> ( num[2] );
}


void date::throw_bounds_error(const char* field, uint val, uint max, const parser* p_lex) {
    if (p_lex != nullptr)
        throw va_error("Cannot represent %s %u (maximum is %u) in date value at %s:L%d",
                       field, val, max, p_lex->pathname(), p_lex->line());
    else
        throw va_error("Cannot represent %s %u (maximum is %u) in date value",
                       field, val, max);
}
