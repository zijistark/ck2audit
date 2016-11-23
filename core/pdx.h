// -*- c++ -*-

#pragma once

#include "lexer.h"
#include "token.h"
#include "error.h"

#include <vector>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <cstdio>


namespace pdx {

    struct block;
    struct list;
    struct plexer;

#pragma pack(push, 1)

    struct date {
        int16_t y;
        int8_t  m;
        int8_t  d;

        date() : y(0), m(0), d(0) {}
        date(int16_t year, int8_t month, int8_t day) : y(year), m(month), d(day) {}
        date(const char*, plexer* p_lex = nullptr);

        bool operator<(date e) const noexcept {
            /* note that since our binary representation is simpy a 32-bit unsigned integer
             * and since our fields are MSB to LSB, we could just compare date_t as a uint,
             * but this is the more correct pattern for multi-field comparison, and I don't
             * feel the need to optimize this. :) */
            if (y < e.y) return true;
            if (e.y < y) return false;
            if (m < e.m) return true;
            if (e.m < y) return false;
            if (d < e.d) return true;
            if (e.d < d) return false;
            return false;
        }

        int16_t year()  const noexcept { return y; }
        int8_t  month() const noexcept { return m; }
        int8_t  day()   const noexcept { return d; }

    private:
        void throw_error(const plexer* p_lex = nullptr);
    };

#pragma pack(pop)

    struct obj {
        enum : uint {
            STRING = 0,
            KEYWORD, // TODO
            INTEGER,
            DECIMAL, // TODO
            DATE,
            BLOCK,
            LIST
        } type;

        union U {
            char*  s;
            int    i;
            block* p_block;
            list*  p_list;
            date   date;
            // uint id;
            U() { }
        } data;

        obj() { type = STRING; data.s = 0; }

        /* KEYWORD & DECIMAL currently don't have default constructors, as they'll
          * have distinct types once implemented */
        obj(char* s)  { type = STRING;  data.s = s; }
        obj(int i)    { type = INTEGER; data.i = i; }
        obj(date d)   { type = DATE;    data.date = d; }
        obj(block* p) { type = BLOCK;   data.p_block = p; }
        obj(list* p)  { type = LIST;    data.p_list = p; }


        /* accessors (unchecked type) */
        char*  as_c_str()   const noexcept { return data.s; }
        int    as_integer() const noexcept { return data.i; }
        block* as_block()   const noexcept { return data.p_block; }
        list*  as_list()    const noexcept { return data.p_list; }
        date   as_date()    const noexcept { return data.date; }
        // uint   as_keyword() const noexcept { return data.id; }
        // char*  as_decimal() const noexcept { return data.s; }

        /* type accessors */
        bool is_c_str()   const noexcept { return type == STRING; }
        bool is_integer() const noexcept { return type == INTEGER; }
        bool is_block()   const noexcept { return type == BLOCK; }
        bool is_list()    const noexcept { return type == LIST; }
        bool is_date()    const noexcept { return type == DATE; }
        // bool is_keyword() const noexcept { return type == KEYWORD; }
        // bool is_decimal() const noexcept { return type == DECIMAL; }

        void print(FILE*, uint indent = 0);
    };

    struct stmt {
        obj key;
        obj val;

        stmt() = delete;
        stmt(obj k, obj v) : key(k), val(v) {}

        bool key_eq(const char* s) const noexcept {
            return (key.type == obj::STRING && strcmp(key.as_c_str(), s) == 0);
        }
        bool key_eq(const std::string& s) const noexcept {
            return (key.type == obj::STRING && s == key.as_c_str());
        }

        void print(FILE*, uint indent = 0);
    };

    class list {
        typedef std::vector<obj> vec_t;
        vec_t vec;

    public:
        list() = delete;
        list(plexer&);

        vec_t::size_type      size() const  { return vec.size(); }
        vec_t::iterator       begin()       { return vec.begin(); }
        vec_t::iterator       end()         { return vec.end(); }
        vec_t::const_iterator begin() const { return vec.cbegin(); }
        vec_t::const_iterator end() const   { return vec.cend(); }
    };

    class block {
        typedef std::vector<stmt> vec_t;
        vec_t vec;

    public:
        block() { }
        block(plexer&, bool is_root = false, bool is_save = false);

        void print(FILE*, uint indent = 0);

        vec_t::size_type      size() const  { return vec.size(); }
        vec_t::iterator       begin()       { return vec.begin(); }
        vec_t::iterator       end()         { return vec.end(); }
        vec_t::const_iterator begin() const { return vec.cbegin(); }
        vec_t::const_iterator end() const   { return vec.cend(); }

    protected:
        static block EMPTY_BLOCK;
    };

    struct plexer : public lexer {
        void next(token*, bool eof_ok = false);
        void next_expected(token*, uint type);
        void unexpected_token(const token&) const;
        void save_and_lookahead(token*);

    private:
        struct saved_token : public token {
            char buf[128];
            saved_token() : token(token::END, &buf[0]) { }
        };

        enum {
            NORMAL, // read from lexer::next(...)
            TOK1,   // read from tok1, then tok2
            TOK2,   // read from tok2, then lexer::next()
        } state;

        saved_token tok1;
        saved_token tok2;

    public:
        plexer() = delete;
        plexer(const fs::path& p) : lexer(p), state(NORMAL) {}
        plexer(const std::string& p) : lexer(p), state(NORMAL) {}
        plexer(const char* p) : lexer(p), state(NORMAL) {}
    };

    static const uint TIER_BARON = 1;
    static const uint TIER_COUNT = 2;
    static const uint TIER_DUKE = 3;
    static const uint TIER_KING = 4;
    static const uint TIER_EMPEROR = 5;

    inline uint title_tier(const char* s) {
        switch (*s) {
        case 'b':
            return TIER_BARON;
        case 'c':
            return TIER_COUNT;
        case 'd':
            return TIER_DUKE;
        case 'k':
            return TIER_KING;
        case 'e':
            return TIER_EMPEROR;
        default:
            return 0;
        }
    }

    bool looks_like_title(const char*);
}
