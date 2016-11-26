
#include "pdx.hpp"
#include "token.hpp"
#include "error.hpp"

#include <ctype.h>
#include <boost/tokenizer.hpp>


using namespace pdx;

pdx::block::block(plexer& lex, bool is_root, bool is_save) {

    if (is_root && is_save) {
        /* skip over CK2txt header (savegames only) */
        token t;
        lex.next_expected(&t, token::STR);
    }

    while (1) {
        token tok;

        lex.next(&tok, is_root);

        if (tok.type == token::END)
            return;

        if (tok.type == token::CLOSE) {
            if (is_root && !is_save) // closing braces are only bad at root level
                throw va_error("Unmatched closing brace in %s (before line %u)",
                               lex.pathname(), lex.line());

            // otherwise, they mean it's time return to the previous block
            return;
        }

        obj key;

        if (tok.type == token::STR)
            key = obj{ lex.strdup(tok.text) };
        else if (tok.type == token::DATE)
            key = obj{ date{ tok.text } };
        else if (tok.type == token::INTEGER)
            key = obj{ atoi(tok.text) };
        else
            lex.unexpected_token(tok);

        /* ...done with key */

        lex.next_expected(&tok, token::EQ);

        /* on to value... */
        obj val;
        lex.next(&tok);

        if (tok.type == token::OPEN) {

            /* need to do token lookahead for 2 tokens to determine whether this is opening a
               generic list or a recursive block of statements */

            lex.next(&tok);
            bool double_open = false;

            if (tok.type == token::CLOSE) {
                /* empty block */
                val = obj{ std::make_unique<block>() };
                continue;
            }
            else if (tok.type == token::OPEN) {
                /* special case for a list of blocks (only matters for savegames) */

                /* NOTE: technically, due to the structure of the language, we could NOT check
                   for a double-open at all and still handle lists of blocks. this is because no
                   well-formed PDX script will ever have an EQ token following an OPEN, so a
                   list is always detected and the lookahead mechanism functions as
                   expected. nevertheless, in the interest of the explicit... */

                double_open = true;
            }

            lex.save_and_lookahead(&tok);

            if (tok.type != token::EQ || double_open)
                val = obj{ std::make_unique<list>(lex) }; // by God, this is (probably) a list!
            else
                val = obj{ std::make_unique<block>(lex) }; // presumably block, so recurse

            /* ... will handle its own closing brace */
        }
        else if (tok.type == token::STR || tok.type == token::QSTR)
            val = obj{ lex.strdup(tok.text) };
        else if (tok.type == token::QDATE || tok.type == token::DATE) {
            /* for savegames, otherwise only on LHS (and never quoted) */
            val = obj{ date{ tok.text } };
        }
        else if (tok.type == token::INTEGER)
            val = obj{ atoi(tok.text) };
        else
            lex.unexpected_token(tok);

        // TODO: RHS (val) should support fixed-point decimal types; I haven't decided whether to make integers
        // and fixed-point decimal all use the same 64-bit type yet.

        vec.emplace_back(key, val);
    }
}

void pdx::block::print(FILE* f, uint indent) {
    for (auto&& s : vec)
        s.print(f, indent);
}


void pdx::stmt::print(FILE* f, uint indent) {
    fprintf(f, "%*s", indent, "");
    key.print(f, indent);
    fprintf(f, " = ");
    val.print(f, indent);
    fprintf(f, "\n");
}


void pdx::obj::print(FILE* f, uint indent) {

    if (type == STRING) {
        if (strpbrk(data.s, " \t\xA0\r\n\'")) // not the only time to quote, but whatever
            fprintf(f, "\"%s\"", data.s);
        else
            fprintf(f, "%s", data.s);
    }
    else if (type == INTEGER)
        fprintf(f, "%d", data.i);
//        else if (type == DECIMAL)
//            fprintf(f, "%s", data.s);
    else if (type == DATE)
        fprintf(f, "%s", data.s);
    else if (type == BLOCK) {
        fprintf(f, "{\n");
        data.up_block->print(f, indent+4);
        fprintf(f, "%*s}", indent, "");
    }
    else if (type == LIST) {
        fprintf(f, "{ ");

        for (auto&& o : *as_list()) {
            o.print(f, indent);
            fprintf(f, " ");
        }

        fprintf(f, "}");
    }
    else
        assert(false);
}

pdx::list::list(plexer& lex) {
    token t;

    while (true) {
        lex.next(&t);

        if (t.type == token::QSTR || t.type == token::STR)
            vec.emplace_back( lex.strdup(t.text) );
        else if (t.type == token::INTEGER)
            vec.emplace_back( atoi(t.text) );
        else if (t.type == token::OPEN) {
            vec.emplace_back( std::make_unique<block>(lex) );
        }
        else if (t.type != token::CLOSE)
            lex.unexpected_token(t);
        else
            return;
    }
}

void pdx::plexer::next_expected(token* p_tok, uint type) {
    next(p_tok);

    if (p_tok->type != type)
        throw va_error("Expected %s token but got token %s at %s:L%d",
                                     token::TYPE_MAP[type], p_tok->type_name(), pathname(), line());
}


void pdx::plexer::unexpected_token(const token& t) const {
    throw va_error("Unexpected token %s at %s:L%d",
                                 t.type_name(), pathname(), line());
}


void pdx::plexer::next(token* p_tok, bool eof_ok) {

    while (1) {

        if (state == NORMAL)
            lexer::next(p_tok);
        else if (state == TOK1) {
            p_tok->type = tok1.type;
            p_tok->text = tok1.text;
            state = TOK2;
        }
        else {
            p_tok->type = tok2.type;
            p_tok->text = tok2.text;
            state = NORMAL;
        }

        /* debug */
        //      printf("%s\n", p_tok->type_name());

        if (p_tok->type == token::END) {
            if (!eof_ok)
                throw va_error("Unexpected EOF at %s:L%d", pathname(), line());
            else
                return;
        }

        if (p_tok->type == token::FAIL)
            throw va_error("Unrecognized token at %s:L%d", pathname(), line());

        if (p_tok->type == token::COMMENT)
            continue;

        return;
    }
}


void pdx::plexer::save_and_lookahead(token* p_tok) {
    /* save our two tokens of lookahead */
    tok1.type = p_tok->type;
    strcpy(tok1.text, p_tok->text); // buffer overflows are myths

    next(p_tok);

    tok2.type = p_tok->type;
    strcpy(tok2.text, p_tok->text);

    /* set lexer to read from the saved tokens first */
    state = TOK1;
}

pdx::date::date(const char* date_str, plexer* p_lex) {
    /* FIXME: I thought this would be cleaner and more standard than using strsep, but I was wrong.
     * current implementation implies a lot of unnecessary copying/allocation (even for an awesome
     * compiler). that's why the date_str parameter is kept mutable in the spec -- so that I can
     * return this to an strpbrk-based inline split. */

    const std::string s{ date_str };

    typedef boost::tokenizer< boost::char_separator<char> > tokenizer_t;
    tokenizer_t tok(s, boost::char_separator<char>(". "));

    auto t = tok.begin();
    if (t == tok.end()) throw_error(p_lex);
    y = atoi(t->c_str());

    ++t;
    if (t == tok.end()) throw_error(p_lex);
    m = atoi(t->c_str());

    ++t;
    if (t == tok.end()) throw_error(p_lex);
    d = atoi(t->c_str());

    if (++t != tok.end()) throw_error(p_lex);
}

void pdx::date::throw_error(const plexer* p_lex) {
    y = 0; m = 0; d = 0;

    if (p_lex != nullptr)
        throw va_error("Malformed date-type expression at %s:L%d", p_lex->pathname(), p_lex->line());
    else
        throw va_error("Malformed date-type expression");
}


/* not handled directly by scanner because there are some things that look like titles
     and are not, but these aberrations (e.g., mercenary composition tags) only appear on
     the RHS of statements. */
bool pdx::looks_like_title(const char* s) {
    if (strlen(s) < 3)
        return false;

    if ( !(*s == 'b' || *s == 'c' || *s == 'd' || *s == 'k' || *s == 'e') )
        return false;

    if (s[1] != '_')
        return false;

    if (!isalpha(s[2])) // eliminates c_<character_id> syntax, among other things
        return false;

    return true;
}
