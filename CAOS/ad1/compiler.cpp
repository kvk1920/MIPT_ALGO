#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
	const char	* name;
	void		* pointer;
} symbol_t;

static uint32_t 	* output;
static const char 	* expr;
static const symbol_t   * extern_symbols;
static size_t 		current_pos;

static const uint32_t 	COND_AL = 0xFE000000;

inline uint32_t
shifted_bit(int i)
{
	return (uint32_t) 1 << i;
}

inline uint32_t
shifted_mask(uint32_t mask, int i)
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

static void write_instruction(uint32_t instruction);
static void mov_pure_value(int reg, uint8_t val, int rot);
static void orr_pure_val(int reg, uint8_t val, int root);
static void mov_val(int reg, int val);
static void mov_reg(int dst, int src);
static void push(uint16_t register_list);
static void pop(uint16_t register_list);
static void add(int dst, int src);
static void sub(int dst, int src);
static void mul(int dst, int src);

static void go_to_next_char();
static void * get_current_symbol();
static void start_compile();
static void finish_compile();

static void func_call();

static void parse_epxr();
static void parse_num();
static void parse_var();
static void parse_function();

void
jit_compile_expression_to_arm(
			const char 	* expression,
			const symbol_t 	* externs,
			void 		* out_buffer
			)
{
	start_compile();
	parse_expr();
	finish_compile();	
}

static void
start_compile()
{
	push(
		shifted_mask(R4) |
		shifted_mask(R5) |
		shifted_mask(R6) |
		shifted_mask(R7) |
		shifted_mask(R8) |
		shifted_mask(R9) |
		shifted_mask(R10)
		);
}

static void
finish_compile()
{
	pop(
		shifted_mask(R4) |
                shifted_mask(R5) |
                shifted_mask(R6) |
                shifted_mask(R7) |
                shifted_mask(R8) |
                shifted_mask(R9) |
                shifted_mask(R10)
		);
	write_instruction(
		COND_AL |
		shifted_bit(24) |
		shifted_bit(21) |
		0x000FFF10 |
		LR
		);
}

static void
go_to_next_char()
{
	while (expr[current_pos] == ' ')
		++current_pos;
}

static void *
get_current_symbol()
{
	go_to_next_char();
	size_t j = current_pos, size_of_name = 0;
	while (expr[current_pos]
		&& expr[current_pos] != '('
		&& expr[current_pos] != '+'
		&& expr[current_pos] != '-'
		&& expr[current_pos] != '*'
		&& expr[current_pos] != ','
		&& expr[current_pos] != ')'
		)
	{
		++current_pos;
		go_to_next_char();
		++size_of_name;
	}
	for (
		const symbol_t * symbol = extern_symbols;
		symbol->name;
		++symbol
		)
		if (strlen(symbol->name) == size_of_name)
		{
			int pos = 0;
			bool same_names = false;
			for (int i = j; pos != size_of_name; ++i)
				if (expr[i] != ' ')
					if (expr[i] != symbol->name[pos])
					{
						same_names = false;
						break;
					}
					else ++pos;
			if (same_names)
				return symbol->pointer;
		}
	return 0;
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
mov_val(int reg, uint32_t val)
{
	mov_pure_val(reg, val & 0x000000FF, 0);
	orr_pure_val(reg, (val & 0xFF000000) >> 24, 4);
	orr_pure_val(reg, (val & 0x00FF0000) >> 16, 8);
	orr_pure_val(reg, (val & 0x0000FF00) >> 8, 12);
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
