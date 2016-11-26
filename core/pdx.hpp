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
#include <string>
#include <boost/filesystem.hpp>


namespace pdx {

    using std::unique_ptr;
    namespace fs = boost::filesystem;

    class vfs {
        std::vector<fs::path> _path_stack;

    public:
        vfs(const fs::path& base_path) : _path_stack({ base_path }) {}
        vfs() {}

        void push_mod_path(const fs::path& p) { _path_stack.push_back(p); }

        bool resolve_path(fs::path* p_real_path, const fs::path& virtual_path) const {
            /* search path vector for a filesystem hit in reverse */
            for (auto i = _path_stack.crbegin(); i != _path_stack.crend(); ++i)
                if (fs::exists( *p_real_path = *i / virtual_path ))
                    return true;
            return false;
        }

        /* a more convenient accessor which auto-throws on a nonexistent path */
        fs::path operator[](const fs::path& virtual_path) const {
            fs::path p;
            if (!resolve_path(&p, virtual_path))
                throw std::runtime_error("Missing game file: " + virtual_path.string());
            return p;
        }

        /* std::string / c-string convenience overloads */

        bool resolve_path(fs::path* p_real_path, const std::string& virtual_path) const {
            return resolve_path(p_real_path, fs::path(virtual_path));
        }

        bool resolve_path(fs::path* p_real_path, const char* virtual_path) const {
            return resolve_path(p_real_path, fs::path(virtual_path));
        }

        fs::path operator[](const std::string& virtual_path) const { return (*this)[fs::path(virtual_path)]; }
        fs::path operator[](const char* virtual_path) const        { return (*this)[fs::path(virtual_path)]; }
    };

    class parser;

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

        bool operator==(const date& e) { return y == e.y && m == e.y && d == e.d; }

        int16_t year()  const noexcept { return y; }
        int8_t  month() const noexcept { return m; }
        int8_t  day()   const noexcept { return d; }

    private:
        void throw_error(const parser* p_lex = nullptr);
    };

#pragma pack(pop)

    class block;
    class list;

    class obj {
        enum {
            STRING,
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

            /* tell C++ that we'll manage the nontrivial union members (the smart pointers) outside of this union */
            data_union() {}
            ~data_union() {}
        } data;

        void destroy() noexcept; // helper for dtor & move-assignment

    public:

        obj(char* s = nullptr)    : type(STRING)  { data.s = s; }
        obj(int i)                : type(INTEGER) { data.i = i; }
        obj(date d)               : type(DATE)    { data.d = d; }
        obj(unique_ptr<block> up) : type(BLOCK)   { new (&data.up_block) unique_ptr<block>(std::move(up)); }
        obj(unique_ptr<list> up)  : type(LIST)    { new (&data.up_list) unique_ptr<list>(std::move(up)); }

        /* move-assignment operator */
        obj& operator=(obj&& other);

        /* move-constructor (implemented via move-assignment) */
        obj(obj&& other) : obj() { *this = std::move(other); }

        /* destructor */
        ~obj() { destroy(); }

        /* data accessors (unchecked type) */
        char*  as_string()  const noexcept { return data.s; }
        int    as_integer() const noexcept { return data.i; }
        date   as_date()    const noexcept { return data.d; }
        block* as_block()   const noexcept { return data.up_block.get(); }
        list*  as_list()    const noexcept { return data.up_list.get(); }

        /* type accessors */
        bool is_string()  const noexcept { return type == STRING; }
        bool is_integer() const noexcept { return type == INTEGER; }
        bool is_date()    const noexcept { return type == DATE; }
        bool is_block()   const noexcept { return type == BLOCK; }
        bool is_list()    const noexcept { return type == LIST; }

        /* convenience equality operator overloads */
        bool operator==(const char* s) const noexcept { return is_string() && strcmp(as_string(), s) == 0; }
        bool operator==(const std::string& s) const noexcept { return is_string() && s == as_string(); }
        bool operator==(int i) const noexcept { return is_integer() && as_integer() == i; }
        bool operator==(date d) const noexcept { return is_date() && as_date() == d; }

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

        void print(FILE*, uint indent = 0);
    };

    class list {
        typedef std::vector<obj> vec_t;
        vec_t _vec;

    public:
        list() = delete;
        list(parser&);

        vec_t::size_type      size() const  { return _vec.size(); }
        vec_t::iterator       begin()       { return _vec.begin(); }
        vec_t::iterator       end()         { return _vec.end(); }
        vec_t::const_iterator begin() const { return _vec.cbegin(); }
        vec_t::const_iterator end() const   { return _vec.cend(); }
    };

    class block {
        typedef std::vector<statement> vec_t;
        vec_t _vec;

    public:
        block() { }
        block(parser&, bool is_root = false, bool is_save = false);

        void print(FILE*, uint indent = 0);

        vec_t::size_type      size() const  { return _vec.size(); }
        vec_t::iterator       begin()       { return _vec.begin(); }
        vec_t::iterator       end()         { return _vec.end(); }
        vec_t::const_iterator begin() const { return _vec.cbegin(); }
        vec_t::const_iterator end() const   { return _vec.cend(); }
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
        } _state;

        saved_token _tok1;
        saved_token _tok2;

        cstr_pool<char> _string_pool;
        unique_ptr<block> _up_root_block;

    protected:
        friend class block;
        friend class list;

        char* strdup(const char* s) { return _string_pool.strdup(s); }

        void next(token*, bool eof_ok = false);
        void next_expected(token*, uint type);
        void unexpected_token(const token&) const;
        void save_and_lookahead(token*);

    public:
        parser() = delete;
        parser(const char* p, bool is_save = false)
            : lexer(p), _state(NORMAL) { _up_root_block = std::make_unique<block>(*this, true, is_save); }
        parser(const std::string& p, bool is_save = false) : parser(p.c_str(), is_save) {}
        parser(const fs::path& p, bool is_save = false) : parser(p.string().c_str(), is_save) {}

        block* root_block() noexcept { return _up_root_block.get(); }
    };

    /* misc. utility functions */

    static const uint TIER_BARON   = 1;
    static const uint TIER_COUNT   = 2;
    static const uint TIER_DUKE    = 3;
    static const uint TIER_KING    = 4;
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
