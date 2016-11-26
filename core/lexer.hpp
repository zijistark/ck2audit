// -*- c++ -*-

#pragma once

#include <cstdio>
#include <string>
#include <memory>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

typedef unsigned int uint;
struct token;

class lexer {
    typedef std::unique_ptr<std::FILE, int (*)(std::FILE *)> unique_file_ptr;
    unique_file_ptr _f;

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
