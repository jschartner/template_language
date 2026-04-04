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

#define todo() do{ asm("int3"); fprintf(stderr, "%s:%d:TODO\n", __FILE__, __LINE__); exit(1); }while(0)

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
    KIND_STRING,
    KIND_ARRAY,
    KIND_OBJECT,
} Kind;

struct Value {
    
    Kind kind;
    union {
        b8 boolean;
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
        case KIND_STRING: {
            //
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
        case KIND_STRING: {
            return value_string(value->as.string);
        } break;
        case KIND_OBJECT: {
            Value *object = value_object();

            Value *curr = value->as.object;
            while(curr) {
    
                value_array_push(object, value_copy(curr));
        
                curr = curr->next;
            }
    
            return object;
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


str content_chop_alphanum(str *content) {

    *content = str_trim_left(*content);
    if(content->len == 0) todo();
    if(!is_alpha(*content->data)) todo();
    return str_chop_while_alphanum(content);
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



b8 find_endfor(str input, str *_content, str *_after) {

    u64 depth = 1;

    u8 *start = input.data;

    while(True) {

        str before, content, after;
        if(!find_content(input, &before, &content, &after)) {
            return False;
        }

        if(depth == 1) {
            if(str_eq(content, str_lit("endfor"))) {

                u8 *end = before.data + before.len;

                *_content = (str) { start, end - start };
                *_after   = after;

                return True;
            } else {

                str maybe_for = content_chop_alphanum(&content);

                // TODO: validate it even more ?
                b8 is_for = str_eq(maybe_for, str_lit("for"));
                if(is_for) {
                    depth += 1;
                } else {
                    // pass
                }

                input = after;

            }
        } else {
            if(str_eq(content, str_lit("endfor"))) {
                todo();
            } else {
                todo();    
            }
        }

    }

}

void expand_value(
    str_builder *sb, 
    str content, 
    Value *dict, 
    str key) {

    Value *value = value_object_get(dict, key);
    if(!value) todo();

    switch(value->kind) {
        case KIND_STRING: {
            str_builder_append(sb, value->as.string);
            content_expect_empty(content);
        } break;

        case KIND_OBJECT: {

            content_chop_string(&content, str_lit("."));
            key = content_chop_alphanum(&content);
    
            // TODO: keep track of your own call-stack?
            expand_value(sb, content, value, key);

        } break;

        default:
            todo();
    }

}

str_builder *expand(str_builder *sb, str input, Value *dict) {

    asm("int3");

    while(True) {

        str before, content, rest;
        if(find_content(input, &before, &content, &rest)) {
            str_builder_append(sb, before);
            input = rest;

            str key = content_chop_alphanum(&content);

            if(key.len == 0) todo();
            if(str_eq(key, str_lit("for"))) {

                str it = content_chop_alphanum(&content);
                str in = content_chop_alphanum(&content);
                str iterator = content_chop_alphanum(&content);
                content_expect_empty(content);

                if(!str_eq(in, str_lit("in"))) {
                    todo();
                }

                Value *array = value_object_get(dict, iterator);
                if(!array) todo();
                if(array->kind != KIND_ARRAY) todo();

                if(value_object_contains_key(dict, it)) {
                    todo();
                }

                str inner_input;
                if(!find_endfor(input, &inner_input, &rest)) {
                    todo();
                }

                Value *curr = array->as.array;

                while(curr) {

                    // Copy the value, since appending it to dict,
                    // invalidates 'curr->next', that points to the
                    // next array_element.
                    Value *copy = value_copy(curr);

                    value_object_push(dict, it, copy);

                    // TODO: keep track of your own call-stack?
                    expand(sb, inner_input, dict);

                    value_free(value_pop(dict));
                    value_free(value_pop(dict));

                    curr = curr->next;

                }

                input = rest;

            } else {

                // TODO: keep track of your own call-stack?
                expand_value(sb, content, dict, key);

            }

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
                    "{{ for obj in objects}}"
                    "Hello, {{ prefix }}.{{ obj.name }}!\n"
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
