
#pragma once
#include "config.hpp"


_PDX_NAMESPACE_BEGIN


class parser;

#pragma pack(push, 1)

class date {
    uint16_t _y;
    uint8_t  _m;
    uint8_t  _d;

    void throw_bounds_error(const char* field, uint val, uint max, const parser* p_lex = nullptr);

public:
    date(char* src, const parser* p_lex = nullptr); // only for use on mutable date-strings known to be well-formed
    date(uint16_t year, uint8_t month, uint8_t day) : _y(year), _m(month), _d(day) {}

    uint16_t year()  const noexcept { return _y; }
    uint8_t  month() const noexcept { return _m; }
    uint8_t  day()   const noexcept { return _d; }

    bool operator<(const date& e) const noexcept {
        if (_y < e._y) return true;
        if (e._y < _y) return false;
        if (_m < e._m) return true;
        if (e._m < _y) return false;
        if (_d < e._d) return true;
        if (e._d < _d) return false;
        return false;
    }

    bool operator==(const date& e) const noexcept { return _y == e._y && _m == e._y && _d == e._d; }
};

#pragma pack(pop)


_PDX_NAMESPACE_END
