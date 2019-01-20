#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    const char	    * name;
    void	   * pointer;
} symbol_t;

static uint32_t 	* output;                               //out_buff
static char 		* expr;                                 //expression without spaces
static const symbol_t   * extern_symbols;                   //externs
static size_t 		current_pos;                            //current processing position

static const uint32_t 	COND_AL = 0xE0000000;               //conditional prefix of instructions

static uint32_t
shifted_bit(int i)                                          //1 << i
{
    return (uint32_t) 1 << i;
}

static uint32_t
shifted_mask(uint32_t mask, int i)                          //mask << i
{
    return (uint32_t) mask << i;
}

enum registers {
    R0 = 0,
    R1 = 1,
    R2 = 2,
    R3 = 3,
    R4 = 4,
    R5 = 5,
    R6 = 6,
    R7 = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,
    SP = 13,
    LR = 14
};

static void write_instruction(uint32_t instruction);        //write instruction to out_buff

//arm instructions
static void mov_pure_value(int reg, uint8_t val, int rot);  //move 8bit-value with rotation (mov reg, #val, #rot)
static void orr_pure_value(int reg, uint8_t val, int rot);  //or 8bit-value with rotation   (orr reg, #val, #rot)
static void mov_val(int reg, int val);                      //move 32bit-value
static void mov_reg(int dst, int src);                      //mov   dst, src
static void push(uint16_t register_list);
static void pop(uint16_t register_list);
static void add(int dst, int src);                          //add dst, dst, src
static void sub(int dst, int src);                          //sub dst, dst, src
static void mul(int dst, int src);                          //mul dst, dst, src
static void ldr(int dst, int src);                          //ldr dst, [src]

//parse and compile functions
static void * get_current_symbol();                         //get pointer to extern symbol with name starting at current_pos and move current_pos aftern the end of name
static void start_compile();                                //push {...}
static void finish_compile();                               //pop {...}, bx lr

static void func_call(void * pointer);                      //call func, save result to R0

/*
 * is_part_of_mul = true, if ... * expr
 * is_after_unary_operator = true, if ... +expr or ... -expr, where + and - are unary
 * result saved to R0
 */
static void parse_expr(bool is_part_of_mul, bool is_after_unary_operator);
static void parse_num();                                    //save decimal constant to R0
static void process_var(void * pointer);                    //save value of var to R0
static void process_function(void * pointer);               //parse func args and call func, save result to R0

static void
parse_num()
{
    int num = 0;
    while (isdigit(expr[current_pos]))
        num = num * 10 + expr[current_pos++] - '0';
    mov_val(R0, num);
}

static void
ldr(int dst, int src)
{
    write_instruction(
            COND_AL |
            shifted_bit(26) |
            shifted_bit(20) |
            shifted_mask(dst, 16) |
            shifted_mask(src, 12)
    );
}

static void
process_var(void * pointer)
{
    mov_val(R0, (uint32_t)pointer);
    ldr(R0, R0);
}

static void
process_function(void * pointer)
{
    ++current_pos;
    size_t argc = 0;
    while (expr[current_pos] != ')')
    {
        ++argc;
        if (expr[current_pos] == ',')
            ++current_pos;
        parse_expr(false, false);
        push(shifted_bit(R0));
    }
    ++current_pos;
    while (argc)
        pop(shifted_bit(--argc));
    func_call(pointer);
}

static void
parse_expr(bool is_part_of_mul, bool is_after_unary_operator)
{
    if (expr[current_pos] == '(')
    {
        ++current_pos;
        parse_expr(false, false);
        ++current_pos;
    }
    else if (expr[current_pos] == '+' || expr[current_pos] == '-')
    {
        char sign = expr[current_pos++];
        parse_expr(false, true);
        if (sign == '-')
        {
            mov_val(R1, -1);
            mul(R0, R1);
        }
    }
    else if (isdigit(expr[current_pos]))
        parse_num();
    else
    {
        void * pointer = get_current_symbol();
        if (expr[current_pos] == '(')
            process_function(pointer);
        else process_var(pointer);
    }
    if (is_after_unary_operator)
        return;
    if (expr[current_pos] == '*')
    {
        ++current_pos;
        push(shifted_bit(R0));
        parse_expr(true, false);
        pop(shifted_bit(R1));
        mul(R0, R1);
    }
    if (!is_part_of_mul && (expr[current_pos] == '+' || expr[current_pos] == '-'))
    {
        char sign = expr[current_pos++];
        push(shifted_bit(R0));
        parse_expr(false, false);
        pop(shifted_bit(R1));
        if (sign == '+')
            add(R0, R1);
        else
        {
            sub(R1, R0);
            mov_reg(R0, R1);
        }
    }
}

static void
func_call(void * pointer)
{
    mov_val(R4, (uint32_t)pointer);
    push(shifted_bit(LR));
    write_instruction(
            COND_AL |
            shifted_bit(24) |
            shifted_bit(21) |
            shifted_mask(0xFFF, 8) |
            shifted_bit(5) |
            shifted_bit(4) |
            R4
    );
    pop(shifted_bit(LR));
}

static void
start_compile()
{
    push(
            shifted_bit(R4) |
            shifted_bit(R5) |
            shifted_bit(R6) |
            shifted_bit(R7) |
            shifted_bit(R8) |
            shifted_bit(R9) |
            shifted_bit(R10)
    );
}

static void
finish_compile()
{
    pop(
            shifted_bit(R4) |
            shifted_bit(R5) |
            shifted_bit(R6) |
            shifted_bit(R7) |
            shifted_bit(R8) |
            shifted_bit(R9) |
            shifted_bit(R10)
    );
    write_instruction(
            COND_AL |
            shifted_bit(24) |
            shifted_bit(21) |
            0x000FFF10 |
            LR
    );
}

static void *
get_current_symbol()
{
    size_t j = current_pos;
    while (expr[current_pos]
           && expr[current_pos] != '('
           && expr[current_pos] != '+'
           && expr[current_pos] != '-'
           && expr[current_pos] != '*'
           && expr[current_pos] != ','
           && expr[current_pos] != ')'
            )
        ++current_pos;
    void * pointer = 0;
    char buff = expr[current_pos];
    expr[current_pos] = '\0';
    for (
            const symbol_t * symbol = extern_symbols;
            symbol->name;
            ++symbol
            )
        if (strcmp(expr + j, symbol->name) == 0)
        {
            pointer = symbol->pointer;
            break;
        }
    expr[current_pos] = buff;
    return pointer;
}

static void
write_instruction(uint32_t instruction)
{
    *(output++) = instruction;
}

static void
mov_pure_value(int reg, uint8_t val, int rot)
{
    write_instruction(
            COND_AL |
            shifted_bit(25) |
            shifted_bit(24) |
            shifted_bit(23) |
            shifted_bit(21) |
            shifted_mask(reg, 12) |
            shifted_mask(rot, 8) |
            val
    );
}

static void
orr_pure_value(int reg, uint8_t val, int rot)
{
    write_instruction(
            COND_AL |
            shifted_bit(25) |
            shifted_bit(24) |
            shifted_bit(23) |
            shifted_mask(reg, 16) |
            shifted_mask(reg, 12) |
            shifted_mask(rot, 8) |
            val
    );
}

static void
mov_val(int reg, int val)
{
    mov_pure_value(reg, val & 0x000000FF, 0);
    orr_pure_value(reg, (val & 0xFF000000) >> 24, 4);
    orr_pure_value(reg, (val & 0x00FF0000) >> 16, 8);
    orr_pure_value(reg, (val & 0x0000FF00) >> 8, 12);
}

static void
mov_reg(int dst, int src)
{
    write_instruction(
            COND_AL |
            shifted_bit(24) |
            shifted_bit(23) |
            shifted_bit(21) |
            shifted_mask(dst, 12) |
            src
    );
}

static void
push(uint16_t register_list)
{
    write_instruction(
            COND_AL |
            shifted_bit(27) |
            shifted_bit(24) |
            shifted_bit(21) |
            shifted_mask(SP, 16) |
            register_list
    );
}

static void
pop(uint16_t register_list)
{
    write_instruction(
            COND_AL |
            shifted_bit(27) |
            shifted_bit(23) |
            shifted_bit(21) |
            shifted_bit(20) |
            shifted_mask(SP, 16) |
            register_list
    );
}

static void
add(int dst, int src)
{
    write_instruction(
            COND_AL |
            shifted_bit(23) |
            shifted_mask(dst, 16) |
            shifted_mask(dst, 12) |
            src
    );
}

static void
sub(int dst, int src)
{
    write_instruction(
            COND_AL |
            shifted_bit(22) |
            shifted_mask(dst, 16) |
            shifted_mask(dst, 12) |
            src
    );
}

static void
mul(int dst, int src)
{
    write_instruction(
            COND_AL |
            shifted_mask(dst, 16) |
            shifted_mask(dst, 8) |
            shifted_bit(7) |
            shifted_bit(4) |
            src
    );
}

void
jit_compile_expression_to_arm(
        const char      * expression,
        const symbol_t  * externs,
        void            * out_buffer
)
{
    expr = malloc(strlen(expression) + 1);
    int i = 0;
    for (int j = 0; expression[j]; ++j)
        if (expression[j] != ' ')
            expr[i++] = expression[j];
    expr[i] = '\0';
    extern_symbols = externs;
    output = out_buffer;

    start_compile();
    parse_expr(false, false);
    finish_compile();

    free(expr);
}

