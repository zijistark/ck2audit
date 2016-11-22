// -*- c++ -*-

#pragma once

#include <cstdio>
#include <string>
#include <boost/filesystem.hpp>

#include "scanner.h"

namespace fs = boost::filesystem;

typedef unsigned int uint;
struct token;

class lexer {
    FILE* _f;

    /* position of last-lexed token */
    uint _line;
    const char* _filename;

public:
    lexer() = delete;
    lexer(const char* path);
    lexer(const fs::path& p) : lexer(p.string().c_str()) { }

    bool next(token* p_tok);

    const char* filename() const noexcept { return _filename; }
    uint line() const noexcept { return _line; }
};
