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
    const char* _pathname;

public:
    lexer() = delete;
    lexer(const char* path);
    lexer(const std::string& path) : lexer(path.c_str()) {}
    lexer(const fs::path& path) : lexer(path.string().c_str()) {}

    bool next(token* p_tok);

    const char* pathname() const noexcept { return _pathname; }
    uint line() const noexcept { return _line; }
};
