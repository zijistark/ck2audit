
#include "pdx.h"
#include "token.h"
#include "error.h"

#include <cstdlib>
#include <string.h>
#include <ctype.h>


namespace pdx {

    block block::EMPTY_BLOCK;

    block::block(plexer& lex, bool is_root, bool is_save) {

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
                                   lex.filename(), lex.line());

                // otherwise, they mean it's time return to the previous block
                return;
            }

            vec.emplace_back();
            stmt& stmt = vec.back();

            if (tok.type == token::STR) {
                stmt.key.data.s = strdup( tok.text );
            }
            else if (tok.type == token::DATE) {
                stmt.key.type = obj::DATE;
                stmt.key.data.s = strdup( tok.text );
                // FIXME: new date encoder plz
                //stmt.key.store_date_from_str(tok.text);
            }
            else if (tok.type == token::INTEGER) {
                stmt.key.type = obj::INTEGER;
                stmt.key.data.i = atoi( tok.text );
            }
            else
                lex.unexpected_token(tok);

            /* ...done with key */

            lex.next_expected(&tok, token::EQ);

            /* on to value... */
            lex.next(&tok);

            if (tok.type == token::OPEN) {

                /* need to do token lookahead for 2 tokens to determine whether this is opening a
                   generic list or a recursive block of statements */

                lex.next(&tok);
                bool double_open = false;

                if (tok.type == token::CLOSE) {
                    /* empty block */

                    stmt.val.type = obj::BLOCK;
                    stmt.val.data.p_block = &EMPTY_BLOCK;

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

                if (tok.type != token::EQ || double_open) {

                    /* by God, this is (probably) a list! */
                    stmt.val.type = obj::LIST;
                    stmt.val.data.p_list = new list(lex);
                }
                else {
                    /* presumably a block */

                    /* time to recurse ... */
                    stmt.val.type = obj::BLOCK;
                    stmt.val.data.p_block = new block(lex);
                }

                /* ... will handle its own closing brace */
            }
            else if (tok.type == token::STR || tok.type == token::QSTR) {
                stmt.val.data.s = strdup( tok.text );
            }
            else if (tok.type == token::QDATE || tok.type == token::DATE) {
                /* for savegames, otherwise only on LHS (and never quoted) */
                stmt.val.type = obj::DATE;
                stmt.val.data.s = strdup( tok.text ); // FIXME: new date encoder plz
            }
            else if (tok.type == token::INTEGER) {
                stmt.val.type = obj::INTEGER;
                stmt.val.data.i = atoi( tok.text );
            }
            else if (tok.type == token::DECIMAL) {
                stmt.val.type = obj::DECIMAL;
                stmt.val.data.s = strdup( tok.text );
            }
            else
                lex.unexpected_token(tok);
        }
    }

    void block::print(FILE* f, uint indent) {
        for (auto&& s : vec)
            s.print(f, indent);
    }


    void stmt::print(FILE* f, uint indent) {
        fprintf(f, "%*s", indent, "");
        key.print(f, indent);
        fprintf(f, " = ");
        val.print(f, indent);
        fprintf(f, "\n");
    }


    void obj::print(FILE* f, uint indent) {

        if (type == STRING) {
            if (strpbrk(data.s, " \t\xA0\r\n\'")) // not the only time to quote, but whatever
                fprintf(f, "\"%s\"", data.s);
            else
                fprintf(f, "%s", data.s);
        }
        else if (type == INTEGER)
            fprintf(f, "%d", data.i);
        else if (type == DECIMAL)
            fprintf(f, "%s", data.s);
        else if (type == DATE)
            fprintf(f, "%s", data.s);
        else if (type == BLOCK) {
            fprintf(f, "{\n");
            data.p_block->print(f, indent+4);
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

    list::list(plexer& lex) {
        token t;

        while (true) {
            obj o;
            lex.next(&t);

            if (t.type == token::QSTR || t.type == token::STR) {
                o.data.s = strdup(t.text);
                vec.push_back(o);
            }
            else if (t.type == token::INTEGER) {
                o.type = obj::INTEGER;
                o.data.i = atoi(t.text);
                vec.push_back(o);
            }
            else if (t.type == token::DECIMAL) {
                o.type = obj::DECIMAL;
                o.data.s = strdup(t.text);
                vec.push_back(o);
            }
            else if (t.type == token::OPEN) {
                o.type = obj::BLOCK;
                o.data.p_block = new block(lex);
                vec.push_back(o);
            }
            else if (t.type != token::CLOSE)
                lex.unexpected_token(t);
            else
                return;
        }
    }

    void plexer::next_expected(token* p_tok, uint type) {
        next(p_tok);

        if (p_tok->type != type)
            throw va_error("Expected %s token but got token %s at %s:L%d",
                                         token::TYPE_MAP[type], p_tok->type_name(), filename(), line());
    }


    void plexer::unexpected_token(const token& t) const {
        throw va_error("Unexpected token %s at %s:L%d",
                                     t.type_name(), filename(), line());
    }


    void plexer::next(token* p_tok, bool eof_ok) {

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
                    throw va_error("Unexpected EOF at %s:L%d", filename(), line());
                else
                    return;
            }

            if (p_tok->type == token::FAIL)
                throw va_error("Unrecognized token at %s:L%d", filename(), line());

            if (p_tok->type == token::COMMENT)
                continue;

            return;
        }
    }


    void plexer::save_and_lookahead(token* p_tok) {
        /* save our two tokens of lookahead */
        tok1.type = p_tok->type;
        strcpy(tok1.text, p_tok->text); // buffer overflows are myths

        next(p_tok);

        tok2.type = p_tok->type;
        strcpy(tok2.text, p_tok->text);

        /* set lexer to read from the saved tokens first */
        state = TOK1;
    }


    /* not handled directly by scanner because there are some things that look like titles
         and are not, but these aberrations (e.g., mercenary composition tags) only appear on
         the RHS of statements. */
    bool looks_like_title(const char* s) {
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
}

