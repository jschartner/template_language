#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

#define todo() do{ fprintf(stderr, "%s:%d:TODO\n", __FILE__, __LINE__); exit(1); }while(0)

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

str str_trim_left(str s) {
    
    while(0 < s.len && is_space(*s.data)) {
        s.data += 1;
        s.len  -= 1;
    }

    return s;
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

Value *value_boolean(b8 boolean) {
    Value *v = alloc(sizeof(*v));
    v->kind = KIND_BOOLEAN;
    v->as.boolean = boolean;
    v->next = NULL;
    return v;
}

Value *value_string(str string) {
    Value *v = alloc(sizeof(*v));
    v->kind = KIND_STRING;
    v->as.string = string;
    v->next = NULL;
    return v;
}

Value *value_array(void) {
    Value *v = alloc(sizeof(*v));
    v->kind = KIND_ARRAY;
    v->as.array = NULL;
    v->next = NULL;
    return v;
}

Value *value_array_push(Value *array, Value *value) {
    value->next = array->as.array;
    array->as.array = value;
    return array;
}

Value *value_object(void) {
    Value *v = alloc(sizeof(*v));
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

    todo();
}

str chop_alphanum(str *content) {

    *content = str_trim_left(*content);
    if(content->len == 0) todo();
    if(!is_alpha(*content->data)) todo();
    return str_chop_while_alphanum(content);
}

void expect_empty(str content) {
    content = str_trim_left(content);
    if(0 < content.len) todo();
}

str expand(str input, Value *dict) {

    u64 start = __mem_pos;

    str prefix = str_lit("{{");
    str suffix = str_lit("}}");

    while(0 < input.len) {

        u64 where;
        if(str_index_of(input, prefix, &where)) {
            memcpy(alloc(input.len), input.data, input.len);
            input.len = 0;
            break;
        }

        str slice = { input.data, where };
        input = (str) { input.data + where + prefix.len, input.len - where - prefix.len };

        memcpy(alloc(slice.len), slice.data, slice.len);

        if(!str_index_of(input, suffix, &where)) {
            todo();
        }

        str content = { input.data, where };
        input = (str) { input.data + where + suffix.len, input.len - where - suffix.len };

        str key = chop_alphanum(&content);

        if(key.len == 0) todo();
        if(str_eq(key, str_lit("for"))) {

            str it = chop_alphanum(&content);
            str in = chop_alphanum(&content);
            str iterator = chop_alphanum(&content);
            expect_empty(content);

            if(!str_eq(in, str_lit("in"))) {
                todo();
            }

            Value *array = value_object_get(dict, iterator);

            todo();

        } else {
            expect_empty(content);

            Value *value = value_object_get(dict, key);
            switch(value->kind) {
                case KIND_STRING: {
                    str s = value->as.string; 
                    memcpy(alloc(s.len), s.data, s.len);
                } break;
                default:
                    todo();
            }
        }


    }

    u64 end = __mem_pos;

    return (str) {
        __mem + start,
        end - start
    };

}

s32 main(void) {

    Value *dict = value_object();

    Value *names = value_array();
    value_array_push( names, value_string(str_lit("foo")) );
    value_array_push( names, value_string(str_lit("bar")) );
    value_array_push( names, value_string(str_lit("bazz")) );

    value_object_push( dict, str_lit("names"), names );

    str data = str_lit(
            "{{ for name in names }}\n"
            "Hello \"{{ name }}\"!\n"
            "{{ endfor }}"
            );

    str s = expand(data, dict);

    printf(str_fmt"\n", str_arg(s)); 


    return 0;
}
