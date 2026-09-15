#ifndef CONSTANTS_H
#define CONSTANTS_H

enum TokenId 
{
    EPSILON  = 0,
    DOLLAR   = 1,
    t_id = 2,
    t_program = 3,
    t_begin = 4,
    t_end = 5,
    t_if = 6,
    t_then = 7,
    t_else = 8,
    t_while = 9,
    t_do = 10,
    t_for = 11,
    t_function = 12,
    t_return = 13,
    t_read = 14,
    t_write = 15,
    t_true = 16,
    t_false = 17,
    t_op_and = 18,
    t_op_or = 19,
    t_op_not = 20,
    t_type_int = 21,
    t_type_float = 22,
    t_type_string = 23,
    t_type_boolean = 24,
    t_type_char = 25,
    t_lit_int_bin = 26,
    t_lit_int_hex = 27,
    t_lit_int_dec = 28,
    t_lit_real = 29,
    t_lit_char = 30,
    t_lit_string = 31,
    t_op_add = 32,
    t_op_sub = 33,
    t_op_mul = 34,
    t_op_div = 35,
    t_op_mod = 36,
    t_op_greater_eq = 37,
    t_op_less_eq = 38,
    t_op_equal = 39,
    t_op_not_equal = 40,
    t_op_greater = 41,
    t_op_less = 42,
    t_op_shift_right = 43,
    t_op_shift_left = 44,
    t_op_bit_and = 45,
    t_op_bit_or = 46,
    t_op_bit_not = 47,
    t_op_bit_xor = 48,
    t_op_assign = 49,
    t_open_paren = 50,
    t_close_paren = 51,
    t_open_bracket = 52,
    t_close_bracket = 53,
    t_open_brace = 54,
    t_close_brace = 55,
    t_comma = 56,
    t_stmt_end = 57
};

const int STATES_COUNT = 88;

extern int SCANNER_TABLE[STATES_COUNT][256];

extern int TOKEN_STATE[STATES_COUNT];

extern int SPECIAL_CASES_INDEXES[59];

extern const char *SPECIAL_CASES_KEYS[18];

extern int SPECIAL_CASES_VALUES[18];

extern const char *SCANNER_ERROR[STATES_COUNT];

const int FIRST_SEMANTIC_ACTION = 99;

const int SHIFT  = 0;
const int REDUCE = 1;
const int ACTION = 2;
const int ACCEPT = 3;
const int GO_TO  = 4;
const int ERROR  = 5;

extern const int PARSER_TABLE[225][99][2];

extern const int PRODUCTIONS[110][2];

extern const char *PARSER_ERROR[225];

#endif
