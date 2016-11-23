
#pragma once

#include <cstring>
#include <stdexcept>
#include <forward_list>

template<const size_t _MAX_STRLEN = 511>
class cstr_pool {
public:
    static const size_t MAX_STRLEN = _MAX_STRLEN;

private:
    struct chunk {
        static const size_t MAX_SZ = _MAX_STRLEN + 1;
        char data[MAX_SZ];
    };

    typedef std::forward_list<chunk> list_t;
    list_t _chunks;
    size_t _end; // index of chunks.front()'s one-past-NUL of last allocated c_str, aka begin of new c_str [0,MAX_STRLEN]

public:
    cstr_pool() : _end(MAX_STRLEN) {}

    char* copy_c_str(const char* src) {
        const size_t len = strlen(src);

        if (len > MAX_STRLEN)
            throw std::runtime_error("cstr_pool::copy_c_str() tried to allocate string larger than maximum chunk length");

        size_t new_end = _end + len + 1;

        if (new_end > MAX_STRLEN) {
            /* use a fresh chunk */
            _chunks.emplace_front();
            _end = 0;
            new_end = len + 1;
        }

        char* dst = _chunks.front().data + _end;
        strcpy(dst, src);
        _end = new_end;
        return dst;
    }
};
