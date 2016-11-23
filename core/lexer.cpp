
#include <cstdio>
#include <cassert>
#include "lexer.hpp"
#include "scanner.h"
#include "token.hpp"
#include "error.hpp"


lexer::lexer(const char* pathname)
    : _f(fopen(pathname, "rb")),
      _line(0),
      _pathname(pathname) {

    if (!_f)
        throw va_error("Could not open file: %s", pathname);

    yyin = _f;
    yylineno = 1;
}


bool lexer::next(token* p_tok) {
    uint type;

    if (( type = yylex() ) == 0) {
        /* EOF, so close our filehandle, and signal EOF */
        fclose(_f);
        _f = 0;
        _line = yylineno;
        p_tok->type = token::END;
        p_tok->text = 0;
        return false;
    }

    /* yytext contains token,
       yyleng contains token length,
       yylineno contains line number,
       type contains token ID */

    _line = yylineno;
    p_tok->type = type;

    if (type == token::QSTR || type == token::QDATE) {
        assert( yyleng >= 2 );

        /* trim quote characters from actual string */
        yytext[ yyleng-1 ] = '\0';
        p_tok->text = yytext + 1;

        return true;
    }
    else {
        p_tok->text = yytext;
    }

    /* if found, strip any trailing '\r' */

    if (yyleng > 0) {
        char* last = &yytext[ yyleng-1 ];

        if (*last == '\r')
            *last = '\0';
    }

    return true;
}
