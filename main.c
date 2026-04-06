#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef uint8_t b8;

#define False 0
#define True 1

#ifdef _WIN32
#  define int3() __debugbreak()
#else
#  define int3() asm("int3")
#endif // _WIN32

#define todo() do{ int3(); fprintf(stderr, "%s:%d:TODO\n", __FILE__, __LINE__); exit(1); }while(0)

u8 __mem[1024 * 1024] = {0};
u64 __mem_pos = 0;

void *alloc(u64 size) {

    u64 new_pos = __mem_pos + size;
    if(sizeof(__mem) <= new_pos) {
        todo();
    }

    void *mem = &__mem[__mem_pos];
    __mem_pos = new_pos;

    return mem;
}

b8 is_space(u8 c) {
    return
        c == ' ' ||
        c == '\n' ||
        c == '\t';
}

b8 is_num(u8 c) {
    return
        '0' <= c && c <= '9';
}

b8 is_alpha(u8 c) {
    return
        ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ;
}

typedef struct {
    u8 *data;
    u64 len;
} str;

#define str_fmt "%.*s"
#define str_arg(s) (s32) (s).len, (s).data
#define str_lit(cstr) (str) { (u8 *) (cstr), sizeof(cstr) - 1 }

b8 str_eq(str a, str b) {
    return a.len == b.len && memcmp(a.data, b.data, b.len) == 0;
}

b8 str_index_of(str haystack, str needle, u64 *where) {
    for(u64 i=0;(i+needle.len)<=haystack.len;i++) {
        if(memcmp(haystack.data + i, needle.data, needle.len) == 0) {
            *where = i;
            return True;
        }
    }

    return False;
}

b8 str_starts_with(str haystack, str prefix) {
    u64 where;
    return str_index_of(haystack, prefix, &where) && where == 0;
}

str str_trim_left(str s) {

    while(0 < s.len && is_space(*s.data)) {
        s.data += 1;
        s.len  -= 1;
    }

    return s;
}

str str_trim_right(str s) {

    while(0 < s.len && is_space(s.data[s.len - 1])) {
        s.len -= 1;
    }

    return s;
}

str str_trim(str s) {
    return str_trim_left(str_trim_right(s));
}

str str_chop_while_alphanum(str *s) {

    u64 i = 0;
    while(True) {
        if(s->len <= i) break;
        u8 c = s->data[i];
        if(is_alpha(c) || is_num(c)) {

        } else {
            break;
        }
        i += 1;
    }

    str result = {
        s->data,
        i
    };

    s->data += i;
    s->len  -= i;

    return result;
}


typedef struct Value Value;

typedef enum {
    KIND_BOOLEAN,
    KIND_NUMBER,
    KIND_STRING,
    KIND_ARRAY,
    KIND_OBJECT,
} Kind;

struct Value {

    Kind kind;
    union {
        b8 boolean;
        s64 number;
        str string;
        Value *array;
        Value *object;
    } as;
    Value *next;
};


static Value *__free_values = NULL;

Value *value_alloc(void) {

    Value *value;
    if(__free_values) {
        value = __free_values;
        __free_values = __free_values->next;
    } else {
        value = alloc(sizeof(*value));
    }
    value->next = NULL;

    return value;
}

void value_free(Value *value) {

    switch(value->kind) {

        case KIND_BOOLEAN:
        case KIND_NUMBER:
        case KIND_STRING:
            break;

        case KIND_OBJECT:
        case KIND_ARRAY: {
            Value *curr = value->as.object;

            while(curr) {
                Value *next = curr->next;
                curr->next = NULL;
                value_free(curr);
                curr = next;
            }

        } break;

        default:
            todo();
    }

    if(value->next) todo();
    value->next = __free_values;
    __free_values = value;

}

Value *value_boolean(b8 boolean) {
    Value *v = value_alloc();
    v->kind = KIND_BOOLEAN;
    v->as.boolean = boolean;
    return v;
}

Value *value_number(s64 number) {
    Value *v = value_alloc();
    v->kind = KIND_NUMBER;
    v->as.number = number;
    return v;
}

Value *value_string(str string) {
    Value *v = value_alloc();
    v->kind = KIND_STRING;
    v->as.string = string;
    return v;
}

Value *value_array(void) {
    Value *v = value_alloc();
    v->kind = KIND_ARRAY;
    v->as.array = NULL;
    return v;
}

Value *value_array_push_single(Value *array, Value *value) {
    value->next = array->as.array;
    array->as.array = value;
    return array;
}

Value *value_array_push_many(
        Value *array,
        ... /* Value *value1, Value *value2, NULL */ ) {

    va_list args;
    va_start(args, array);

    while(True) {
        Value *value = va_arg(args, Value *);
        if(!value) break;

        value_array_push_single(array, value);
    }

    va_end(args);

    return array;
}

#define value_array_push(a, ...) value_array_push_many((a), __VA_ARGS__, NULL)

Value *value_pop(Value *array) {

    Value *curr = array->as.array;
    if(!curr) todo();
    array->as.array = curr->next;
    curr->next = NULL;
    return curr;

}
#define value_array_pop value_pop

Value *value_object(void) {
    Value *v = value_alloc();
    v->kind = KIND_OBJECT;
    v->as.object = NULL;
    v->next = NULL;
    return v;
}

Value *value_object_push(Value *object, str key, Value *value) {
    value_array_push(object, value);
    value_array_push(object, value_string(key));
    return object;
}

Value *value_object_get(Value *object, str name) {

    Value *curr = object->as.object;

    while(curr) {

        str key = curr->as.string;
        curr = curr->next;

        Value *value = curr;
        curr = curr->next;

        if(str_eq(name, key)) {
            return value;
        }

    }

    return NULL;
}

b8 value_object_contains_key(Value *object, str name) {
    return value_object_get(object, name) != NULL;
}


Value *value_copy(Value *value) {

    switch(value->kind) {
        case KIND_BOOLEAN: {
            return value_boolean(value->as.boolean);
        } break;
        case KIND_NUMBER: {
            return value_number(value->as.number);
        } break;
        case KIND_STRING: {
            return value_string(value->as.string);
        } break;
        case KIND_OBJECT: {
            Value *object = value_object();

            Value *curr = value->as.object;
            while(curr) {

                Value *key = curr;
                curr = curr->next;

                value_array_push(object, value_copy(curr));
                value_array_push(object, value_copy(key));

                curr = curr->next;
            }

            return object;
        } break;
        case KIND_ARRAY: {

            Value *array = value_array();

            Value *curr = value->as.array;
            while(curr) {

                value_array_push(array, value_copy(curr));

                curr = curr->next;
            }

            return array;

        } break;
        default:
            todo();
    }

}

typedef struct str_node str_node;

struct str_node {
    str string;
    str_node *next;
};

typedef struct {
    str_node *head;
    str_node *tail;
    u64 len;
} str_builder;

void str_builder_append(str_builder *sb, str s) {

    str_node *node = alloc(sizeof(*node));
    node->string = s;
    node->next = NULL;

    if(sb->head) {
        sb->tail->next = node;
        sb->tail = node;
    } else {
        sb->head = node;
        sb->tail = node;
    }

    sb->len += s.len;

}

str str_builder_to_str(str_builder sb) {

    u8 *mem = alloc(sb.len);
    u8 *mem_copy = mem;

    str_node *node = sb.head;
    while(node) {
        str s = node->string;

        memcpy(mem, s.data, s.len);
        mem += s.len;

        node = node->next;
    }

    return (str) { mem_copy, sb.len };

}

Value *value_array_get(Value *array, u64 index) {

    Value *curr = array->as.array;

    for(u64 i=0;i<index;i++) {
        if(curr == NULL) todo();
        curr = curr->next;
    }

    if(!curr) todo();
    return curr;

}

u64 value_array_len(Value *array) {
    u64 len = 0;

    Value *curr = array->as.array;
    while(curr) {
        len += 1;
        curr = curr->next;
    }

    return len;
}

#define TOKENS()\
    X("==", double_equals)\
    X(".", dot)\
    X("[", bracket_open)\
    X("]", bracket_close)\
    X("(", paren_open)\
    X(")", paren_close)\
    X("if", if)\
    X("endif", endif)\
    X("for", for)\
    X("endfor", endfor)\
    X(",", comma)\
    X("_", underscore)

typedef enum {
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_ALPHANUM,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
#define X(s, n) TOKEN_TYPE_##n,
    TOKENS()
#undef X
} Token_Type;

typedef struct {
    union {
        s64 number;
        str string;
    } as;
    Token_Type type;
} Token;

Token content_chop_token(str *content) {

    *content = str_trim_left(*content);

    if(content->len == 0) {
        return (Token) {
            .type = TOKEN_TYPE_EOF,
        };
    }

#define X(s, n) do{\
    str delim = str_lit( s );\
    if(delim.len <= content->len && \
         memcmp(content->data, delim.data, delim.len) == 0) {\
        Token token = {\
            .type = TOKEN_TYPE_##n,\
        };\
        content->data += delim.len;\
        content->len  -= delim.len;\
        return token;\
    }\
}while(0);
        TOKENS()
#undef X


    u8 c = *content->data;
    if(is_alpha(c)) {
        u64 i = 0;
        while(True) {
            if(content->len <= i) break;
            c = content->data[i];
            if(is_alpha(c) || is_num(c) || c == '_') {

            } else {
                break;
            }
            i += 1;
        }
        Token result = {
            .type = TOKEN_TYPE_ALPHANUM,
            .as.string = (str) {
                content->data,
                i
            }
        };
        content->data += i;
        content->len  -= i;
        return result;
    } else if(is_num(c)) {
        u64 i = 0;
        s64 number = 0;
        while(True) {
            if(content->len <= i) break;
            c = content->data[i];
            if(is_num(c)) {
                number += c - '0';
            } else {
                break;
            }
            i += 1;
        }
        Token result = {
            .type = TOKEN_TYPE_NUMBER,
            .as.number = number,
        };
        content->data += i;
        content->len  -= i;
        return result;
    } else if(c == '\"') {

        b8 copied = False;

        u64 cap;
        u8 *mem;
        u64 len;

        u64 i = 1;

        b8 running = True;
        b8 escape = False;
        while(running) {
            if(content->len <= i) break;
            c = content->data[i];
            // TODO: add escaping

            switch(c) {
                case '\\': {

                    if(escape) {
                        if(cap <= len) {
                            alloc(cap);
                            cap *= 2;
                        }
                        mem[len++] = '\\';
                        escape = False;
                    } else {
                        if(!copied) {
                            cap = (i - 1) * 2;
                            mem = alloc(cap);
                            len = 0;

                            memcpy(mem, content->data + 1, i - 1);
                            len += i - 1;

                            copied = True;
                        }
                        escape = True;
                    }

                } break;
                case '\"': {
                    if(escape) {
                        if(cap <= len) {
                            alloc(cap);
                            cap *= 2;
                        }
                        mem[len++] = '\"';
                        escape = False;
                    } else {
                        running = False;
                    }
                } break;
                default: {
                    if(copied) {
                        if(cap <= len) {
                            alloc(cap);
                            cap *= 2;
                        }
                        mem[len++] = content->data[i];
                    }
                } break;
            }

            i += 1;
        }
        Token result = {
            .type = TOKEN_TYPE_STRING,
        };
        if(copied) {
            __mem_pos -= cap - len;

            result.as.string = (str) {
                mem,
                len
            };
        } else {
            result.as.string = (str) {
                content->data + 1,
                i - 2
            };
        }
        content->data += i;
        content->len  -= i;
        return result;

    } else {

        todo();
    }

}

str content_chop_alphanum(str *content) {

    Token token = content_chop_token(content);
    if(token.type != TOKEN_TYPE_ALPHANUM) todo();
    return token.as.string;

}

void content_chop_string(str *content, str string) {

    *content = str_trim_left(*content);
    if(!str_starts_with(*content, string)) {
        todo();
    }
    content->data += string.len;
    content->len  -= string.len;

}

void content_expect_empty(str content) {
    content = str_trim_left(content);
    if(0 < content.len) todo();
}

b8 find_content(
        str input,

        str *before,
        str *content,
        str *after

        ) {

    str prefix = str_lit("{{");
    str suffix = str_lit("}}");

    u64 where;
    if(!str_index_of(input, prefix, &where)) {
        return False;
    }

    *before = (str) { input.data, where };
    input = (str) {
        input.data + where + prefix.len,
            input.len  - where - prefix.len
    };

    if(!str_index_of(input, suffix, &where)) {
        return False;
    }

    *content = str_trim( (str) { input.data, where } );
    *after = (str) {
        input.data + where + suffix.len,
            input.len  - where - suffix.len
    };

    return True;
}

b8 find_suffix(
        str input,
        str *_content,
        str *_after,
        Token_Type prefix,
        Token_Type suffix) {

    u64 depth = 1;

    u8 *start = input.data;

    while(True) {

        str before, content, after;
        if(!find_content(input, &before, &content, &after)) {
            return False;
        }

        Token token = content_chop_token(&content);

        if(token.type == suffix) {
            if(depth == 1) {
                u8 *end = before.data + before.len;

                *_content = (str) { start, end - start };
                *_after   = after;

                return True;
            } else {
                depth -= 1;
            }
        } else {
            if(token.type == prefix) {
                depth += 1;
            } else {
                // pass
            }
        }

        input = after;

    }

}

Value *expand_get_value(str *content, Value *dict, Value *global_dict, Token *_token) {

    u32 iteration = 0;

    b8 can_recover = False;
    str recovery;
    while(True) {

        Token token;
        if(_token) {
            token = *_token;
            _token = NULL;
            can_recover = False;
        } else {
            recovery = *content;
            can_recover = True;
            token = content_chop_token(content);
        }

        b8 got_string = False;
        str string;

        b8 got_number = False;
        s64 number;

        b8 got_key = False;
        str key;

        b8 got_index = False;
        u64 index;

        b8 got_value = False;
        Value *value;

        switch(token.type) {
            case TOKEN_TYPE_ALPHANUM: {

                str label = token.as.string;
                if(str_eq(label, str_lit("true"))) {
                    return value_boolean(1);
                } else if(str_eq(label, str_lit("false"))) {
                    return value_boolean(0);
                } else if(str_eq(label, str_lit("len"))) {
                    token = content_chop_token(content);
                    if(token.type != TOKEN_TYPE_paren_open) todo();

                    Value *expr = expand_get_value(content, global_dict, global_dict, NULL);
                    switch(expr->kind) {
                        case KIND_STRING: {
                            number = expr->as.string.len;
                            got_number = True;
                        } break;
                        case KIND_ARRAY: {
                            number = value_array_len(expr);
                            got_number = True;
                        } break;
                        default:
                            todo();
                    }

                    token = content_chop_token(content);
                    if(token.type != TOKEN_TYPE_paren_close) todo();

                    value_free(expr);

                } else {
                    key = label;
                    got_key = True;
                }

            } break;

            case TOKEN_TYPE_NUMBER: {
                number = token.as.number;
                got_number = True;
            } break;

            case TOKEN_TYPE_STRING: {
                string = token.as.string;
                got_string = True;
            } break;

            case TOKEN_TYPE_bracket_open: {

                Value *expr = expand_get_value(content, global_dict, global_dict, NULL);
                switch(expr->kind) {
                    case KIND_STRING: {
                        key = expr->as.string;
                        got_key = True;
                    } break;
                    case KIND_NUMBER: {
                        if(expr->as.number< 0) todo();
                        index = (u64) expr->as.number;
                        got_index = True;
                    } break;
                    default:
                        todo();
                }

                token = content_chop_token(content);
                if(token.type != TOKEN_TYPE_bracket_close) todo();

                value_free(expr);

            } break;

            default: {
                if(iteration == 0) {
                    todo();
                } else {
                    if(!can_recover) todo();
                    *content = recovery;
                    return dict;
                }
            } break;
        }

        if(got_index) {
            if(value->kind != KIND_ARRAY) todo();

            value = value_array_get(dict, index);
            if(!value) {
                todo();
            }
            got_value = True;

        }

        if(got_key) {
            if(dict->kind != KIND_OBJECT) todo();

            value = value_object_get(dict, key);
            if(!value) {
                todo();
            }
            got_value = True;
        }

        if(got_value) {
            switch(value->kind) {
                case KIND_BOOLEAN: {
                    return value_boolean(value->as.boolean);
                } break;
                case KIND_NUMBER: {
                    number = value->as.number;
                    got_number = True;
                } break;
                case KIND_STRING: {
                    string = value->as.string;
                    got_string = True;
                } break;
                case KIND_OBJECT: {
                    token = content_chop_token(content);
                    switch(token.type) {
                        case TOKEN_TYPE_dot:
                            break;
                        case TOKEN_TYPE_bracket_open:
                            // foo["bar"]

                            // Reuse 'token' but update 'dict'
                            _token = &token;
                            break;
                        default:
                            todo();
                    }
                    dict = value;
                } break;
                case KIND_ARRAY: {
                    dict = value;
                } break;
                default:
                    todo();
            }

            iteration += 1;
        }

        if(got_string) {
            str lhs = string;

            str content_copy = *content;
            token = content_chop_token(content);
            switch(token.type) {
                case TOKEN_TYPE_double_equals: {

                    Value *rhs = expand_get_value(content, global_dict, global_dict, NULL);
                    switch(rhs->kind) {
                        case KIND_STRING: {
                            value_free(rhs);
                            return value_boolean(str_eq(lhs, rhs->as.string));
                        } break;
                        default:
                            todo();
                    }

                } break;
                case TOKEN_TYPE_EOF: {
                    return value_string(lhs);
                } break;
                default: {
                    *content = content_copy;
                    return value_string(lhs);
                } break;
            }
        }

        if(got_number) {
            s64 lhs = number;

            str content_copy = *content;
            token = content_chop_token(content);
            switch(token.type) {
                case TOKEN_TYPE_double_equals: {
                    Value *rhs = expand_get_value(content, global_dict, global_dict, NULL);

                    switch(rhs->kind) {
                        case KIND_NUMBER: {
                            value_free(rhs);
                            return value_boolean(lhs == rhs->as.number);
                        } break;
                        default:
                            todo();
                    }
                } break;
                case TOKEN_TYPE_EOF: {
                    return value_number(lhs);
                } break;
                default: {
                    *content = content_copy;
                    return value_number(lhs);
                } break;
            }
        }
    }

}

void value_expand(str_builder *sb, Value *value) {
    switch(value->kind) {
        case KIND_STRING: {
            str_builder_append(sb, value->as.string);
        } break;
        case KIND_NUMBER: {

            s64 number = value->as.number;

            b8 negative = False;
            if(number < 0) {
                number = -number;
                negative = True;
            }

            u32 cap = 32;
            u8 *mem = alloc(cap);
            u32 pos = cap - 1;

            if(number == 0) {
                mem[pos--] = '0';
            } else {
                while(0 < number) {
                    mem[pos--] = (number % 10) + '0';
                    number /= 10;
                }
            }

            if(negative) {
                mem[pos--] = '-';
            }
            pos += 1;

            // __mem_pos -= pos;

            str_builder_append(sb, (str) {
                mem + pos,
                cap - pos
            });
        } break;
        default:
            todo();
    }
}

str_builder *expand(str_builder *sb, str input, Value *dict) {

    while(True) {

        str before, content, rest;
        if(find_content(input, &before, &content, &rest)) {
            str_builder_append(sb, before);
            input = rest;

            Token token = content_chop_token(&content);

            switch(token.type) {
                case TOKEN_TYPE_if: {
                    Value *value = expand_get_value(&content, dict, dict, NULL);
                    content_expect_empty(content);

                    b8 good;
                    if(value->kind == KIND_BOOLEAN) {
                        good = value->as.boolean;
                    } else {
                        todo();
                    }

                    str inner_input;
                    if(!find_suffix(input, &inner_input, &rest, TOKEN_TYPE_if, TOKEN_TYPE_endif)) {
                        todo();
                    }

                    if(good) {
                        expand(sb, inner_input, dict);
                    }

                    input = rest;
                } break;
                case TOKEN_TYPE_for: {

                    u32 arity = 1;
                    Token fst = content_chop_token(&content);
                    Token snd;
                    str in;

                    token = content_chop_token(&content);
                    if(token.type == TOKEN_TYPE_ALPHANUM) {
                        in = token.as.string;
                        // pass
                    } else if(token.type == TOKEN_TYPE_comma) {
                        arity = 2;
                        snd = content_chop_token(&content);
                        in = content_chop_alphanum(&content);
                    } else {
                        todo();
                    }
                    if(!str_eq(in, str_lit("in"))) {
                        todo();
                    }
                    str iterator = content_chop_alphanum(&content);
                    content_expect_empty(content);

                    Value *array = value_object_get(dict, iterator);
                    if(!array) todo();
                    if(array->kind != KIND_ARRAY) todo();

                    b8 set_value;
                    b8 set_index;
                    str value_name;
                    str index_name;
                    if(arity == 1) {
                        value_name = fst.as.string;
                        set_value = fst.type == TOKEN_TYPE_ALPHANUM;
                        set_index = False;
                    } else if(arity == 2) {
                        value_name = snd.as.string;
                        index_name = fst.as.string;
                        set_value = snd.type == TOKEN_TYPE_ALPHANUM;
                        set_index = fst.type == TOKEN_TYPE_ALPHANUM;
                    } else {
                        todo();
                    }

                    if(set_value) {
                        if(value_object_contains_key(dict, value_name)) {
                            todo();
                        }
                    }
                    if(set_index) {
                        if(value_object_contains_key(dict, index_name)) {
                            todo();
                        }
                    }

                    str inner_input;
                    if(!find_suffix(
                                input,
                                &inner_input,
                                &rest,
                                TOKEN_TYPE_for,
                                TOKEN_TYPE_endfor)) {
                        todo();
                    }

                    Value *curr = array->as.array;

                    u64 index = 0;
                    while(curr) {

                        // Copy the value, since appending it to dict,
                        // invalidates 'curr->next', that points to the
                        // next array_element.

                        if(set_value) {
                            Value *copy = value_copy(curr);
                            value_object_push(dict, value_name, copy);
                        }

                        if(set_index) {
                            value_object_push(dict, index_name, value_number(index));
                        }

                        // TODO: keep track of your own call-stack?
                        expand(sb, inner_input, dict);

                        if(set_index) {
                            value_free(value_pop(dict));
                            value_free(value_pop(dict));
                        }

                        if(set_value) {
                            value_free(value_pop(dict));
                            value_free(value_pop(dict));
                        }

                        curr = curr->next;
                        index += 1;

                    }

                    input = rest;
                } break;

                default: {
                    Value *value = expand_get_value(&content, dict, dict, &token);
                    value_expand(sb, value);
                    value_free(value);
                } break;
            }

            content_expect_empty(content);

        } else {
            str_builder_append(sb, input);
            break;
        }

    }

    return sb;
}

s32 main(void) {

    Value *tests = value_array();

    {
        Value *given    = value_string(str_lit(
                    "{{ for index, _ in names }}"
                    "Hello, ({{ index }})!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, (0)!\n"
                    "Hello, (1)!\n"
                    "Hello, (2)!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Bazz")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Foo"))

                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }


    {
        Value *given    = value_string(str_lit(
                    "{{ for index, name in names }}"
                    "Hello, {{ name }} ({{ index }})!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, Foo (0)!\n"
                    "Hello, Bar (1)!\n"
                    "Hello, Bazz (2)!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Bazz")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Foo"))

                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ len(\"foo\") }}"
                    ));
        Value *expected = value_string(str_lit("3"));
        Value *context  = value_object();

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ len(names) }}"
                    ));
        Value *expected = value_string(str_lit("3"));
        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Bazz")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Foo"))

                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for digit in digits }}"
                    "{{ digit }}\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit("0\n1\n2\n"));
        Value *context  = value_object_push(
                value_object(),
                str_lit("digits"),
                value_array_push(
                    value_array(),
                    value_number(2),
                    value_number(1),
                    value_number(0)
                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ names[2] }}"
                    ));
        Value *expected = value_string(str_lit("Bazz"));
        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Bazz")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Foo"))

                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ \"\\\"Hello, \\\\ World!\\\"\" }}"
                    ));
        Value *expected = value_string(str_lit("\"Hello, \\ World!\""));
        Value *context  = value_object();

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit("Hello, {{ data.inner[inner_data_name] }}!"));
        Value *expected = value_string(str_lit("Hello, Foo!"));
        Value *context  = value_object_push(
                value_object(),
                str_lit("inner_data_name"),
                value_string(str_lit("name"))
                );

        value_object_push(
                context,
                str_lit("data"),

                value_object_push(
                    value_object(),
                    str_lit("inner"),
                    value_object_push(
                        value_object(),
                        str_lit("name"),
                        value_string(str_lit("Foo")))

                    )

                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for obj in objects}}"
                    "Hello, {{ [\"name with spaces\"] }}.{{ obj[\"name\"] }}!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, PackageName.Foo!\n"
                    "Hello, PackageName.Bar!\n"
                    "Hello, PackageName.Bazz!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("objects"),
                value_array_push(
                    value_array(),
                    value_object_push(value_object(), str_lit("name"), value_string(str_lit("Bazz"))),
                    value_object_push(value_object(), str_lit("name"), value_string(str_lit("Bar"))),
                    value_object_push(value_object(), str_lit("name"), value_string(str_lit("Foo")))
                    )
                );

        value_object_push(
                context,
                str_lit("name with spaces"),
                value_string(str_lit("PackageName"))
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ \"{{\" }}"
                    ));
        Value *expected = value_string(str_lit("{{"));
        Value *context  = value_object();

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ if name == \"Bar\" }}Its bar\n{{ endif }}"
                    "{{ if \"Foo\" == name }}Its foo\n{{ endif }}"
                    ));
        Value *expected = value_string(str_lit("Its bar\n"));
        Value *context  = value_object_push(
                value_object(),
                str_lit("name"),
                value_string(str_lit("Bar")));;

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit("{{ if 1 == 1 }}Hello, World!{{ endif }}"));
        Value *expected = value_string(str_lit("Hello, World!"));
        Value *context  = value_object();

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit("{{ if true }}Hello, World!{{ endif }}"));
        Value *expected = value_string(str_lit("Hello, World!"));
        Value *context  = value_object();

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for obj in objects}}"
                    "{{ if obj.enabled }}"
                    "Hello, {{ name }}.{{ obj.name }}!\n"
                    "{{ endif }}"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, PackageName.Foo!\n"
                    "Hello, PackageName.Bazz!\n"
                    ));

        Value *obj1 = value_object();
        value_object_push(
                obj1,
                str_lit("name"),
                value_string(str_lit("Bazz")));
        value_object_push(
                obj1,
                str_lit("enabled"),
                value_boolean(1));

        Value *obj2 = value_object();
        value_object_push(
                obj2,
                str_lit("name"),
                value_string(str_lit("Bar")));
        value_object_push(
                obj2,
                str_lit("enabled"),
                value_boolean(0));

        Value *obj3 = value_object();
        value_object_push(
                obj3,
                str_lit("name"),
                value_string(str_lit("Foo")));
        value_object_push(
                obj3,
                str_lit("enabled"),
                value_boolean(1));


        Value *context  = value_object_push(
                value_object(),
                str_lit("objects"),
                value_array_push(value_array(), obj1, obj2, obj3)
                );

        value_object_push(
                context,
                str_lit("name"),
                value_string(str_lit("PackageName"))
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for xs in xss }}"
                    "{{ for x in xs }}"
                    "Hello, {{ x }}!\n"
                    "{{ endfor }}"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, One!\n"
                    "Hello, Two!\n"
                    "Hello, Three!\n"
                    "Hello, Four!\n"
                    "Hello, Five!\n"
                    "Hello, Six!\n"
                    "Hello, Seven!\n"
                    "Hello, Eight!\n"
                    "Hello, Nine!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("xss"),
                value_array_push(
                    value_array(),
                    value_array_push(
                        value_array(),
                        value_string(str_lit("Seven")),
                        value_string(str_lit("Eight")),
                        value_string(str_lit("Nine"))),
                    value_array_push(
                        value_array(),
                        value_string(str_lit("Four")),
                        value_string(str_lit("Five")),
                        value_string(str_lit("Six"))),
                    value_array_push(
                        value_array(),
                        value_string(str_lit("One")),
                        value_string(str_lit("Two")),
                        value_string(str_lit("Three")))
                    )
                    );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for obj in objects}}"
                    "Hello, {{ name }}.{{ obj.name }}!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, PackageName.Foo!\n"
                    "Hello, PackageName.Bar!\n"
                    "Hello, PackageName.Bazz!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("objects"),
                value_array_push(
                    value_array(),
                    value_object_push(value_object(), str_lit("name"), value_string(str_lit("Bazz"))),
                    value_object_push(value_object(), str_lit("name"), value_string(str_lit("Bar"))),
                    value_object_push(value_object(), str_lit("name"), value_string(str_lit("Foo")))
                    )
                );

        value_object_push(
                context,
                str_lit("name"),
                value_string(str_lit("PackageName"))
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for name in names }}"
                    "Hello, {{ prefix }}.{{ name }}!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, PackageName.Foo!\n"
                    "Hello, PackageName.Bar!\n"
                    "Hello, PackageName.Bazz!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Bazz")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Foo"))

                    )
                );

        value_object_push(
                context,
                str_lit("prefix"),
                value_string(str_lit("PackageName"))
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for name in names }}"
                    "Hello, {{ name }}!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello, Foo!\n"
                    "Hello, Bar!\n"
                    "Hello, Bazz!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Bazz")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Foo"))

                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit(
                    "{{ for name in names }}"
                    "Hello!\n"
                    "{{ endfor }}"
                    ));
        Value *expected = value_string(str_lit(
                    "Hello!\n"
                    "Hello!\n"
                    "Hello!\n"
                    ));

        Value *context  = value_object_push(
                value_object(),
                str_lit("names"),
                value_array_push(
                    value_array(),
                    value_string(str_lit("Foo")),
                    value_string(str_lit("Bar")),
                    value_string(str_lit("Bazz"))
                    )
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit("Hello, {{ name }}!"));
        Value *expected = value_string(str_lit("Hello, Foo!"));
        Value *context  = value_object_push(
                value_object(),
                str_lit("name"),
                value_string(str_lit("Foo"))
                );

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }

    {
        Value *given    = value_string(str_lit("Hello, World!"));
        Value *expected = value_string(str_lit("Hello, World!"));
        Value *context  = value_object();

        value_array_push(
                tests,
                value_array_push(value_array(), context, expected, given)
                );
    }


    Value *curr = tests->as.array;

    while (curr) {

        Value *test = curr;

        Value *given    = value_array_pop(test);
        Value *expected = value_array_pop(test);
        Value *context  = value_array_pop(test);

        if(given->kind != KIND_STRING) todo();
        if(expected->kind != KIND_STRING) todo();
        if(context->kind != KIND_OBJECT) todo();

        str got = str_builder_to_str(*expand(
                    &(str_builder) {0},
                    given->as.string,
                    context
                    ));
        printf("Got: '"str_fmt"'\n", str_arg(got));

        if(!str_eq(got, expected->as.string)) {
            printf("    Given: '"str_fmt"'\n", str_arg(given->as.string));
            printf("    Expected: '"str_fmt"'\n", str_arg(expected->as.string));
            todo();
        }

        printf("\n");

        curr = curr->next;
    }

    return 0;
}
