// -*- c++ -*-

#pragma once

#include "lexer.hpp"
#include "token.hpp"
#include "error.hpp"
#include "cstr_pool.hpp"

#include <vector>
#include <cstdint>
#include <cstdio> // deprecated
#include <memory>


namespace pdx {

    using std::unique_ptr;

    struct block;
    struct list;
    struct parser;

#pragma pack(push, 1)

    struct date {
        int16_t y;
        int8_t  m;
        int8_t  d;

        date() : y(0), m(0), d(0) {}
        date(int16_t year, int8_t month, int8_t day) : y(year), m(month), d(day) {}
        date(const char*, parser* p_lex = nullptr);

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
        void throw_error(const parser* p_lex = nullptr);
    };

#pragma pack(pop)

    class obj {
        enum {
            STRING = 0,
            INTEGER,
            DATE,
            BLOCK,
            LIST
        } type;

        union data_union {
            char*  s;
            int    i;
            date   d;
            unique_ptr<block> up_block;
            unique_ptr<list>  up_list;

            data_union() {}
            ~data_union() {}
        } data;

        void destroy() noexcept { // helper method for dtor & move-assignment operator
            switch (type) {
                case STRING:
                case INTEGER:
                case DATE:
                    break;
                case BLOCK: data.up_block.~unique_ptr<block>(); break;
                case LIST:  data.up_list.~unique_ptr<list>(); break;
            }
        }

    public:

        obj(char* s = nullptr)    : type(STRING)  { data.s = s; }
        obj(int i)                : type(INTEGER) { data.i = i; }
        obj(date d)               : type(DATE)    { data.d = d; }
        obj(unique_ptr<block> up) : type(BLOCK)   { new (&data.up_block) unique_ptr<block>(std::move(up)); }
        obj(unique_ptr<list> up)  : type(LIST)    { new (&data.up_list) unique_ptr<list>(std::move(up)); }

        /* move-assignment operator */
        obj& operator=(obj&& other) {
            if (this == &other) return *this; // guard against self-assignment
            destroy(); // destroy our current resources
            /* move resources from other & return self */
            type = other.type;
            switch (other.type) {
                case STRING:  data.s = other.data.s; break;
                case INTEGER: data.i = other.data.i; break;
                case DATE:    data.d = other.data.d; break;
                case BLOCK:   new (&data.up_block) unique_ptr<block>(std::move(other.data.up_block)); break;
                case LIST:    new (&data.up_list)  unique_ptr<list>(std::move(other.data.up_list)); break;
            }
            return *this;
        }

        /* move-constructor (implemented via move-assignment) */
        obj(obj&& other) : obj() { *this = std::move(other); }

        /* destructor */
        ~obj() { destroy(); }

        /* data accessors (unchecked type) */
        char*  as_c_str()   const noexcept { return data.s; }
        int    as_integer() const noexcept { return data.i; }
        date   as_date()    const noexcept { return data.d; }
        block* as_block()   const noexcept { return data.up_block.get(); }
        list*  as_list()    const noexcept { return data.up_list.get(); }

        /* type accessors */
        bool is_c_str()   const noexcept { return type == STRING; }
        bool is_integer() const noexcept { return type == INTEGER; }
        bool is_date()    const noexcept { return type == DATE; }
        bool is_block()   const noexcept { return type == BLOCK; }
        bool is_list()    const noexcept { return type == LIST; }

        void print(FILE*, uint indent = 0);
    };

    class statement {
        obj _k;
        obj _v;

    public:
        statement() = delete;
        statement(obj& k, obj& v) : _k(std::move(k)), _v(std::move(v)) {}

        const obj& key()   const noexcept { return _k; }
        const obj& value() const noexcept { return _v; }

        bool key_eq(const char* s) const noexcept { return _k.is_c_str() && strcmp(_k.as_c_str(), s) == 0; }
        bool key_eq(const std::string& s) const noexcept { return _k.is_c_str() && s == _k.as_c_str(); }

        void print(FILE*, uint indent = 0);
    };

    class list {
        typedef std::vector<obj> vec_t;
        vec_t vec;

    public:
        list() = delete;
        list(parser&);

        vec_t::size_type      size() const  { return vec.size(); }
        vec_t::iterator       begin()       { return vec.begin(); }
        vec_t::iterator       end()         { return vec.end(); }
        vec_t::const_iterator begin() const { return vec.cbegin(); }
        vec_t::const_iterator end() const   { return vec.cend(); }
    };

    class block {
        typedef std::vector<statement> vec_t;
        vec_t vec;

    public:
        block() { }
        block(parser&, bool is_root = false, bool is_save = false);

        void print(FILE*, uint indent = 0);

        vec_t::size_type      size() const  { return vec.size(); }
        vec_t::iterator       begin()       { return vec.begin(); }
        vec_t::iterator       end()         { return vec.end(); }
        vec_t::const_iterator begin() const { return vec.cbegin(); }
        vec_t::const_iterator end() const   { return vec.cend(); }
    };

    class parser : public lexer {
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

        cstr_pool<char> string_pool;

    protected:
        friend class block;
        friend class list;

        char* strdup(const char* src) { return string_pool.strdup(src); }

    public:
        parser() = delete;
        parser(const char* p) : lexer(p), state(NORMAL) {}
        parser(const std::string& p) : parser(p.c_str()) {}
        parser(const fs::path& p) : parser(p.string().c_str()) {}

        /* allocate space for src, copy src, return pointer to copy. allocation is from string_pool associated
         * with this parser (could theoretically be a parameter of parser, shared among more parsers, but eh).
         * this practice allows us to cheaply allocate a bunch of small strings and even more cheaply deallocate
         * all of them when this parser is destroyed.
         */

        void next(token*, bool eof_ok = false);
        void next_expected(token*, uint type);
        void unexpected_token(const token&) const;
        void save_and_lookahead(token*);
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
