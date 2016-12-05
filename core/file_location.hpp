
#pragma once
#include "pdx_common.hpp"


_PDX_NAMESPACE_BEGIN


struct file_location {
    const char* _pathname;
    uint _line;

    file_location() = delete;
    file_location(const char* path, uint line) : _pathname(path), _line(line) {}

    const char* pathname() const noexcept { return _pathname; }
    uint line() const noexcept { return _line; }
};


_PDX_NAMESPACE_END
