/*
  Hatari - breakcond.c

  Copyright (c) 2009 by Eero Tamminen

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  breakcond.c - code for breakpoint conditions that can check variable
  and memory values against each other, mask them etc. before deciding
  whether the breakpoint should be triggered.  See BreakCond_Help()
  for the syntax.
*/
const char BreakCond_fileid[] = "Hatari breakcond.c : " __DATE__ " " __TIME__;

#include <ctype.h>
#include <stdlib.h>
#include "config.h"
#include "main.h"
#include "dsp.h"
#include "debugui.h"
#include "stMemory.h"
#include "breakcond.h"

/* set to 1 to enable parsing function tracing / debug output */
#define DEBUG 0

/* this is in 3 dsp*.c files, maybe it should be in a common header? */
#define BITMASK(x)      ((1<<(x))-1)

#define BC_MAX_CONDITION_BREAKPOINTS 16
#define BC_MAX_CONDITIONS_PER_BREAKPOINT 4

/* defines for how the value needs to be accessed: */
typedef enum {
	BC_TYPE_NUMBER,		/* use as-is */
	BC_TYPE_ADDRESS		/* read from (Hatari) address as-is */
} bc_addressing_t;

typedef struct {
	bc_addressing_t type;
	bool is_indirect;
	bool use_dsp;
	char space;	/* TODO: DSP has P, X, Y address spaces */
	int bits;
	union {
		Uint32 number;
		/* e.g. registers have different sizes */
		Uint32 *addr32;
		Uint16 *addr16;
		Uint8 *addr8;
	} value;
	Uint32 mask;	/* <width mask> && <value mask> */
} bc_value_t;

typedef enum {
	BC_NEXT_AND,	/* next condition is ANDed to current one */
	BC_NEXT_NEW	/* next condition starts a new breakpoint */
} bc_next_t;

typedef struct {
	bc_value_t lvalue;
	bc_value_t rvalue;
	char comparison;
} bc_condition_t;

typedef struct {
	char *expression;
	bc_condition_t conditions[BC_MAX_CONDITIONS_PER_BREAKPOINT];
	int ccount;
} bc_breakpoint_t;

static bc_breakpoint_t BreakPointsCpu[BC_MAX_CONDITION_BREAKPOINTS];
static bc_breakpoint_t BreakPointsDsp[BC_MAX_CONDITION_BREAKPOINTS];
static int BreakPointCpuCount;
static int BreakPointDspCount;


/* --------------------- debugging code ------------------- */

#if DEBUG
/* see parsing code for usage examples */
static int _traceIndent;
static void _spaces(void)
{
	int spaces = _traceIndent;
	while(spaces-- > 0) {
		putchar(' ');	/* fputc(' ',stdout); */
	}
}
#define ENTERFUNC(args) { _traceIndent += 2; _spaces(); printf args ; fflush(stdout); }
#define EXITFUNC(args) { _spaces(); printf args ; fflush(stdout); _traceIndent -= 2; }
#else
#define ENTERFUNC(args)
#define EXITFUNC(args)
#endif


/* ------------- breakpoint condition checking, internals ------------- */

/**
 * Return value from given DSP memory space/address
 */
static Uint32 BreakCond_ReadDspMemory(Uint32 addr, const bc_value_t *bc_value)
{
#warning "TODO: BreakCond_ReadDspMemory()"
	fprintf(stderr, "TODO: BreakCond_ReadDspMemory()\n");
	return 0;
}

/**
 * Return value of given size read from given ST memory address
 */
static Uint32 BreakCond_ReadSTMemory(Uint32 addr, const bc_value_t *bc_value)
{
	switch (bc_value->bits) {
	case 32:
		return STMemory_ReadLong(addr);
	case 16:
		return STMemory_ReadWord(addr);
	case 8:
		return STMemory_ReadByte(addr);
	default:
		fprintf(stderr, "ERROR: unknown ST address size %d!\n", bc_value->bits);
		abort();
	}
}


/**
 * Return Uint32 value according to given bc_value_t specification
 */
static Uint32 BreakCond_GetValue(const bc_value_t *bc_value)
{
	Uint32 value;
	
	switch (bc_value->type) {
	case BC_TYPE_NUMBER:
		value = bc_value->value.number;
		break;
	case BC_TYPE_ADDRESS:
		switch (bc_value->bits) {
		case 32:
			value = *(bc_value->value.addr32);
			break;
		case 16:
			value = *(bc_value->value.addr16);
			break;
		case 8:
			value = *(bc_value->value.addr8);
			break;
		default:
			fprintf(stderr, "ERROR: unknown register size %d!\n", bc_value->bits);
			abort();
		}
		break;
	default:
		fprintf(stderr, "ERROR: unknown breakpoint condition value type %d!\n", bc_value->type);
		abort();
	}
	if (bc_value->is_indirect) {
		if (bc_value->use_dsp) {
			value = BreakCond_ReadDspMemory(value, bc_value);
		} else {
			value = BreakCond_ReadSTMemory(value, bc_value);
		}
	}
	return (value & bc_value->mask);
}


/**
 * Return true if any of the given breakpoint conditions match
 */
static bool BreakCond_MatchConditions(const bc_condition_t *condition, int count)
{
	Uint32 lvalue, rvalue;
	bool hit = false;
	int i;
	
	for (i = 0; i < count; condition++, i++) {

		lvalue = BreakCond_GetValue(&(condition->lvalue));
		rvalue = BreakCond_GetValue(&(condition->rvalue));

		switch (condition->comparison) {
		case '<':
			hit = (lvalue < rvalue);
			break;
		case '>':
			hit = (lvalue > rvalue);
			break;
		case '=':
			hit = (lvalue == rvalue);
			break;
		case '!':
			hit = (lvalue != rvalue);
			break;
		default:
			fprintf(stderr, "ERROR: Unknown breakpoint value comparison operator '%c'!\n",
				condition->comparison);
			abort();
		}
		if (!hit) {
			return false;
		}
	}
	/* all conditions matched */
	return true;
}


/**
 * Return true if any of the given condition breakpoints match
 */
static bool BreakCond_MatchBreakPoints(const bc_breakpoint_t *bp, int count)
{
	int i;
	
	for (i = 0; i < count; bp++, i++) {
		if (BreakCond_MatchConditions(bp->conditions, bp->ccount)) {
			fprintf(stderr, "Breakpoint '%s' matched.\n", bp->expression);
			return true;
		}
	}
	return false;
}

/* ------------- breakpoint condition checking, public API ------------- */

/**
 * Return true if any of the CPU breakpoint/conditions match
 */
bool BreakCond_MatchCpu(void)
{
	return BreakCond_MatchBreakPoints(BreakPointsCpu, BreakPointCpuCount);
}

/**
 * Return true if any of the DSP breakpoint/conditions match
 */
bool BreakCond_MatchDsp(void)
{
	return BreakCond_MatchBreakPoints(BreakPointsDsp, BreakPointDspCount);
}

/**
 * Return number of condition breakpoints
 */
int BreakCond_BreakPointCount(bool bForDsp)
{
	if (bForDsp) {
		return BreakPointDspCount;
	} else {
		return BreakPointCpuCount;
	}
}


/* -------------- breakpoint condition parsing, internals ------------- */

/**
 * If given string is register name (for DSP or CPU), set bc_value
 * fields accordingly and return true, otherwise return false.
 */
static bool BreakCond_ParseRegister(const char *regname, bc_value_t *bc_value)
{
	Uint32 *regaddr;

#warning "TODO: BreakCond_ParseRegister(): support special registers?"
	ENTERFUNC(("BreakCond_ParseRegister('%s')\n", regname));
	if (bc_value->use_dsp) {
		size_t bits;
		bits = DSP_GetRegisterAddress(regname, &regaddr);
		if (bits) {
			bc_value->value.addr32 = regaddr;
			bc_value->bits = bits;  /* TODO: or 32? */
			bc_value->mask = BITMASK(bits);
			EXITFUNC(("-> true (DSP)\n"));
			return true;
		}
		/* check DSP register names */
		EXITFUNC(("-> false (unsupported DSP register?)\n"));
		return false;
	}
	regaddr = DebugUI_GetCpuRegisterAddress(regname);
	if (regaddr) {
		bc_value->value.addr32 = regaddr;
		bc_value->bits = 32;
		bc_value->mask = 0xffffffff;
		EXITFUNC(("-> true (CPU)\n"));
		return true;
	}
	EXITFUNC(("-> false (unsupported 16-bit CPU SR register?)\n"));
	return false;
}

/**
 * If given address is valid (for DSP or CPU), return true.
 */
static bool BreakCond_CheckAddress(bc_value_t *bc_value)
{
	Uint32 addr = bc_value->value.number;
	ENTERFUNC(("BreakCond_CheckAddress(%x)\n", addr));
	if (bc_value->use_dsp) {
		if (addr > 0xFFFF) {
			EXITFUNC(("-> false (DSP)\n"));
			return false;
		}
		EXITFUNC(("-> true (DSP)\n"));
		return true;
	}
	if ((addr > STRamEnd && addr < 0xe00000) || addr > 0xff0000) {
		EXITFUNC(("-> false (CPU)\n"));
		return false;
	}
	EXITFUNC(("-> true (CPU)\n"));
	return true;
}


/* struct for passing around breakpoint conditions parsing state */
typedef struct {
	int arg;		/* current arg */
	int argc;		/* arg count */
	const char **argv;	/* arg pointer array (+ strings) */
	const char *error;	/* error from parsing args */
} parser_state_t;


/**
 * Parse a number, decimal unless prefixed with '$' which signifies hex.
 * Modify pstate according to parsing (arg index and error string).
 * Return true for success and false for error.
 */
static bool BreakCond_ParseNumber(parser_state_t *pstate, const char *value, Uint32 *number)
{
#warning "TODO: BreakCond_ParseNumber(): support specifying DSP memory space?"
	ENTERFUNC(("BreakCond_ParseNumber('%s'), arg:%d\n", value, pstate->arg));
	if (value[0] == '$') {
		if (sscanf(value+1, "%x", number) != 1) {
			pstate->error = "invalid hexadecimal value";
			EXITFUNC(("-> false\n"));
			return false;
		}
	} else {
		if (sscanf(value, "%u", number) != 1) {
			pstate->error = "invalid value";
			EXITFUNC(("-> false\n"));
			return false;
		}
	}
	EXITFUNC(("-> number:%x, true\n", *number));
	return true;
}


/**
 * Check for and parse a condition value size.
 * Modify pstate according to parsing (arg index and error string).
 * Return true for no or successfully parsed size and false for error.
 */
static bool BreakCond_ParseSizeModifier(parser_state_t *pstate, bc_value_t *bc_value)
{
	int size;

	ENTERFUNC(("BreakCond_ParseSizeModifier()\n"));
	if (pstate->arg+2 > pstate->argc ||
	    strcmp(pstate->argv[pstate->arg], ".") != 0) {
		EXITFUNC(("arg:%d -> true (missing)\n", pstate->arg));
		return true;
	}
	pstate->arg++;
	switch (pstate->argv[pstate->arg][0]) {
	case 'l':
		size = 32;
		break;
	case 'w':
		size = 16;
		break;
	case 'b':
		size = 8;
		break;
	default:
		pstate->error = "invalid size modifier";
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	if (pstate->argv[pstate->arg][1]) {
		pstate->error = "invalid size modifier";
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	if (bc_value->type == BC_TYPE_NUMBER && !bc_value->is_indirect) {
		pstate->error = "width modifier doesn't make sense for a number";
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	bc_value->bits = size;
	EXITFUNC(("arg:%d -> size:%d, true\n", pstate->arg, size));
	pstate->arg++;
	return true;
}


/**
 * Check for and parse a condition value mask.
 * Modify pstate according to parsing (arg index and error string).
 * Return true for no or successfully parsed mask and false for error.
 */
static bool BreakCond_ParseMaskModifier(parser_state_t *pstate, bc_value_t *bc_value)
{
	ENTERFUNC(("BreakCond_ParseMaskModifier()\n"));
	if (pstate->arg+2 > pstate->argc ||
	    strcmp(pstate->argv[pstate->arg], "&") != 0) {
		EXITFUNC(("arg:%d -> true (missing)\n", pstate->arg));
		return true;
	}
	/* if you really want to have mask on plain numbers, remove this check */
	if (bc_value->type == BC_TYPE_NUMBER && !bc_value->is_indirect) {
		pstate->error = "numbers shouldn't need masks";
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	pstate->arg++;
	if (!BreakCond_ParseNumber(pstate, pstate->argv[pstate->arg], &(bc_value->mask))) {
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	if (bc_value->mask == 0 ||
	    (bc_value->type == BC_TYPE_NUMBER && !bc_value->is_indirect &&
	     bc_value->value.number && !(bc_value->value.number & bc_value->mask))) {
		pstate->error = "mask zeroes value";
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	EXITFUNC(("arg:%d -> true (%x)\n", pstate->arg, bc_value->mask));
	pstate->arg++;
	return true;
}


/**
 * Parse a breakpoint condition value.
 * Modify pstate according to parsing (arg index and error string).
 * Return true for success and false for error.
 */
static bool BreakCond_ParseValue(parser_state_t *pstate, bc_value_t *bc_value)
{
	const char *str;
	int skip = 1;

	ENTERFUNC(("BreakCond_Value()\n"));
	if (pstate->arg >= pstate->argc) {
		pstate->error = "value missing";
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	/* parse indirection */
	if (pstate->arg+3 <= pstate->argc) {
		if (strcmp(pstate->argv[pstate->arg+0], "(") == 0 &&
		    strcmp(pstate->argv[pstate->arg+2], ")") == 0) {
			bc_value->is_indirect = true;
			pstate->arg++;
			skip = 2;
		}
	}

	/* set default modifiers, register parsing can change them later */
	bc_value->bits = 32;
	bc_value->mask = 0xffffffff;

	str = pstate->argv[pstate->arg];
	/* parse direct or indirect value */
	if (isalpha(str[0])) {
		if (BreakCond_ParseRegister(str, bc_value)) {
			/* address of register */
			bc_value->type = BC_TYPE_ADDRESS;
		} else {
			pstate->error = "invalid register name";
			EXITFUNC(("arg:%d -> false\n", pstate->arg));
			return false;
		}
	} else {
		/* a number */
		bc_value->type = BC_TYPE_NUMBER;
		if (!BreakCond_ParseNumber(pstate, str, &(bc_value->value.number))) {
			EXITFUNC(("arg:%d -> false\n", pstate->arg));
			return false;
		}
		/* suitable as emulated memory address (indirect)? */
		if (bc_value->is_indirect &&
		    !BreakCond_CheckAddress(bc_value)) {
			pstate->error = "invalid address";
			EXITFUNC(("arg:%d -> false\n", pstate->arg));
			return false;
		}
	}
	pstate->arg += skip;

	/* parse modifiers */
	if (!BreakCond_ParseSizeModifier(pstate, bc_value)) {
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	if (!BreakCond_ParseMaskModifier(pstate, bc_value)) {
		EXITFUNC(("arg:%d -> false\n", pstate->arg));
		return false;
	}
	EXITFUNC(("arg:%d -> true (%s value)\n", pstate->arg,
		  (bc_value->is_indirect ? "indirect" : "direct")));
	return true;
}


/**
 * Parse a breakpoint comparison character.
 * Modify pstate according to parsing (arg index and error string).
 * Return the character or nil for an error.
 */
static char BreakCond_ParseComparison(parser_state_t *pstate)
{
	const char *comparison;
	
	ENTERFUNC(("BreakCond_ParseComparison(), arg:%d\n", pstate->arg));
	if (pstate->arg >= pstate->argc) {
		pstate->error = "breakpoint comparison missing";
		EXITFUNC(("-> false\n"));
		return false;
	}
	comparison = pstate->argv[pstate->arg];
	switch (comparison[0]) {
	case '<':
	case '>':
	case '=':
	case '!':
		break;
	default:
		pstate->error = "invalid comparison character";
		EXITFUNC(("-> false\n"));
		return false;
	}
	if (comparison[1]) {
		pstate->error = "trailing comparison character(s)";
		EXITFUNC(("-> false\n"));
		return false;
	}

	pstate->arg++;
	if (pstate->arg >= pstate->argc) {
		pstate->error = "right side missing";
		EXITFUNC(("-> false\n"));
		return false;
	}
	EXITFUNC(("-> '%c'\n", *comparison));
	return *comparison;
}


/**
 * Check that masks and address sizes allow comparison with the other side.
 * If yes, return true, otherwise false.
 */
static bool BreakCond_CrossCheckValues(parser_state_t *pstate,
				       bc_value_t *bc_value1,
				       bc_value_t *bc_value2)
{
	Uint32 mask;
	ENTERFUNC(("BreakCond_CrossCheckValues()\n"));
	if ((bc_value1->mask & bc_value2->mask) == 0) {
		pstate->error = "values masks cancel each other";
		EXITFUNC(("-> false\n"));
		return false;
	}
	if (bc_value1->bits == 32 ||
	    bc_value2->type != BC_TYPE_NUMBER ||
	    bc_value2->is_indirect ||
	    bc_value2->value.number == 0) {
		EXITFUNC(("-> true (no problematic direct types)\n"));
		return true;
	}
	mask = BITMASK(bc_value1->bits) & bc_value1->mask;
	if (bc_value2->value.number > mask) {
		pstate->error = "number is larger than the address width/mask";
		EXITFUNC(("-> false\n"));
		return false;
	}
	EXITFUNC(("-> true\n"));
	return true;
}


/**
 * Parse given breakpoint conditions and append them to breakpoints.
 * Modify pstate according to parsing (arg index and error string).
 * Return number of added conditions or zero for failure.
 */
static int BreakCond_ParseCondition(parser_state_t *pstate, bool bForDsp,
				    bc_condition_t *conditions, int ccount)
{
	bc_condition_t condition;

	ENTERFUNC(("BreakCond_ParseCondition(...)\n"));
	if (ccount >= BC_MAX_CONDITIONS_PER_BREAKPOINT) {
		pstate->error = "max number of conditions exceeded";
		EXITFUNC(("-> false (no conditions free)\n"));
		return false;
	}

	/* setup condition */
	memset(&condition, 0, sizeof(bc_condition_t));
	if (bForDsp) {
		condition.lvalue.use_dsp = true;
		condition.rvalue.use_dsp = true;
	}

	/* parse condition */
	if (!BreakCond_ParseValue(pstate, &(condition.lvalue))) {
		EXITFUNC(("-> false\n"));
		return false;
	}
	condition.comparison = BreakCond_ParseComparison(pstate);
	if (!condition.comparison) {
		EXITFUNC(("-> false\n"));
		return false;
	}
	if (!BreakCond_ParseValue(pstate, &(condition.rvalue))) {
		EXITFUNC(("-> false\n"));
		return false;
	}
	if (!(BreakCond_CrossCheckValues(pstate, &(condition.lvalue), &(condition.rvalue)) &&
	      BreakCond_CrossCheckValues(pstate, &(condition.rvalue), &(condition.lvalue)))) {
		EXITFUNC(("-> false\n"));
		return false;
	}
	/* new condition */
	conditions[ccount++] = condition;

	/* continue with next condition? */
	if (pstate->arg == pstate->argc) {
		EXITFUNC(("-> true (condition %d)\n", ccount-1));
		return true;
	}
	if (strcmp(pstate->argv[pstate->arg], "&&") != 0) {
		pstate->error = "trailing content for breakpoint condition";
		EXITFUNC(("-> false\n"));
		return false;
	}
	pstate->arg++;

	/* recurse conditions parsing */
	ccount = BreakCond_ParseCondition(pstate, bForDsp, conditions, ccount);
	if (!ccount) {
		EXITFUNC(("-> false\n"));
		return false;
	}
	EXITFUNC(("-> true (condition %d)\n", ccount-1));
	return ccount;
}


/**
 * Tokenize given breakpoint expression to given parser struct.
 * Return normalized expression string that corresponds to tokenization
 * or NULL on error. On error, pstate->error contains the error message
 * and pstate->arg index to invalid character (instead of to token like
 * after parsing).
 */
static char *BreakCond_TokenizeExpression(const char *expression,
					  parser_state_t *pstate)
{
	char separator[] = {
		'=', '!', '<', '>',  /* comparison operators */
		'(', ')', '.', '&',  /* other separators */
		'\0'                 /* terminator */
	};
	bool is_separated, has_comparison;
	char sep, *dst, *normalized;
	const char *src;
	int i, tokens;

	memset(pstate, 0, sizeof(parser_state_t));

	/* _minimum_ safe size for normalized expression is 2x+1 */
	normalized = malloc(2*strlen(expression)+1);
	if (!normalized) {
		pstate->error = "alloc failed";
		return NULL;
	}

	/* check characters & normalize string */
	dst = normalized;
	is_separated = false;
	has_comparison = false;
	for (src = expression; *src; src++) {
		/* discard white space in source */
		if (isspace(*src)) {
			continue;
		}
		/* separate tokens with single space in destination */
		for (i = 0; (sep = separator[i]); i++) {
			if (*src == sep) {
				if (dst > normalized) {
					/* don't separate boolean AND '&&' */
					if (*src == '&' && *(src-1) == '&') {
						dst--;
					} else {
						if (!is_separated) {
							*dst++ = ' ';
						}
					}
				}
				*dst++ = *src;
				*dst++ = ' ';
				is_separated = true;
				if (i < 4) {
					has_comparison = true;
				}
				break;
			}
		}
		/* validate & copy other characters */
		if (!sep) {
			if (!(isalnum(*src) || isblank(*src) || *src == '$')) {
				pstate->error = "invalid character";
				pstate->arg = src-expression;
				free(normalized);
				return NULL;
			}
			*dst++ = tolower(*src);
			is_separated = false;
		}
	}
	if (is_separated) {
		dst--;	/* no trailing space */
	}
	*dst = '\0';

	if (!has_comparison) {
		pstate->error = "condition comparison missing";
		pstate->arg = strlen(expression)/2;
		free(normalized);
		return NULL;
	}

	/* allocate exact space for tokenized string array + strings */
	tokens = 1;
	for (dst = normalized; *dst; dst++) {
		if (*dst == ' ') {
			tokens++;
		}
	}
	pstate->argv = malloc(tokens*sizeof(char*)+strlen(normalized)+1);
	if (!pstate->argv) {
		pstate->error = "alloc failed";
		free(normalized);
		return NULL;
	}
	/* and copy/tokenize... */
	dst = (char*)(pstate->argv) + tokens*sizeof(char*);
	strcpy(dst, normalized);
	pstate->argv[0] = strtok(dst, " ");
	for (i = 1; (dst = strtok(NULL, " ")); i++) {
		pstate->argv[i] = dst;
	}
	assert(i == tokens);
	pstate->argc = tokens;
#if DEBUG
	fprintf(stderr, "args->");
	for (i = 0; i < tokens; i++) {
		fprintf(stderr, " %d: %s,", i, pstate->argv[i]);
	}
	fprintf(stderr, "\n");
#endif
	return normalized;
}


/**
 * Helper to set corrent breakpoint list and type name to given variables.
 * Return pointer to breakpoint list count
 */
static int* BreakCond_GetListInfo(bc_breakpoint_t **bp,
				  const char **name, bool bForDsp)
{
	int *bcount;
	if (bForDsp) {
		bcount = &BreakPointDspCount;
		*bp = BreakPointsDsp;
		*name = "DSP";
	} else {
		bcount = &BreakPointCpuCount;
		*bp = BreakPointsCpu;
		*name = "CPU";
	}
	return bcount;
}


/**
 * Parse given breakpoint expression and store it.
 * Return true for success and false for failure.
 */
static bool BreakCond_Parse(const char *expression, bool bForDsp)
{
	parser_state_t pstate;
	bc_breakpoint_t *bp;
	const char *name;
	char *normalized;
	int *bcount;
	int ccount;

	bcount = BreakCond_GetListInfo(&bp, &name, bForDsp);
	if (*bcount >= BC_MAX_CONDITION_BREAKPOINTS) {
		fprintf(stderr, "ERROR: no free %s condition breakpoints left.\n", name);
		return false;
	}
	bp += *bcount;
	
	normalized = BreakCond_TokenizeExpression(expression, &pstate);
	if (normalized) {
		bp->expression = normalized;
		ccount = BreakCond_ParseCondition(&pstate, bForDsp,
						  bp->conditions, 0);
		bp->ccount = ccount;
	} else {
		ccount = 0;
	}
	if (pstate.argv) {
		free(pstate.argv);
	}
	if (ccount > 0) {
		(*bcount)++;
		fprintf(stderr, "%s condition breakpoint %d added.\n",
			name, *bcount);
	} else {
		if (normalized) {
			int offset, i = 0;
			char *s = normalized;
			while (*s && i < pstate.arg) {
				if (*s++ == ' ') {
					i++;
				}
			}
			offset = s - normalized;
			/* show tokenized string and point out
			 * the token where the error was encountered
			 */
			fprintf(stderr, "ERROR in tokenized string:\n'%s'\n%*c-%s\n",
				normalized, offset+2, '^', pstate.error);
			free(normalized);
		} else {
			/* show original string and point out the character
			 * where the error was encountered
			 */
			fprintf(stderr, "ERROR in parsed string:\n'%s'\n%*c-%s\n",
				expression, pstate.arg+2, '^', pstate.error);
		}
	}
	return (ccount > 0);
}


/**
 * List condition breakpoints
 */
static void BreakCond_List(bool bForDsp)
{
	const char *name;
	bc_breakpoint_t *bp;
	int i, bcount;
	
	bcount = *BreakCond_GetListInfo(&bp, &name, bForDsp);
	if (!bcount) {
		fprintf(stderr, "No conditional %s breakpoints.\n", name);
		return;
	}

	fprintf(stderr, "Conditional %s breakpoints:\n", name);
	for (i = 1; i <= bcount; bp++, i++) {
		fprintf(stderr, "%3d: %s\n", i, bp->expression);
	}
}

/**
 * Remove condition breakpoint at given position
 */
static bool BreakCond_Remove(int position, bool bForDsp)
{
	const char *name;
	bc_breakpoint_t *bp;
	int *bcount, offset;
	
	bcount = BreakCond_GetListInfo(&bp, &name, bForDsp);
	if (position < 1 || position > *bcount) {
		fprintf(stderr, "ERROR: No such %s breakpoint.\n", name);
		return false;
	}
	offset = position - 1;
	fprintf(stderr, "Removed %s breakpoint %d:\n  %s\n",
		name, position, bp[offset].expression);
	free(bp[offset].expression);
	if (position < *bcount) {
		memmove(bp+offset, bp+position,
			(*bcount-position)*sizeof(bc_breakpoint_t));
	}
	(*bcount)--;
	return true;
}


/**
 * help
 */
static void BreakCond_Help(void)
{
	fprintf(stderr,
"  breakpoint = <expression> [ && <expression> [ && <expression> ] ... ]\n"
"  expression = <value>[.width] [& <number>] <condition> <value>[.width]\n"
"\n"
"  where:\n"
"  	value = [(] <register-name | number> [)]\n"
"  	number = [$]<digits>\n"
"  	condition = '<' | '>' | '=' | '!'\n"
"  	width = 'b' | 'w' | 'l'\n"
"\n"
"  If the value is in parenthesis like in '($ff820)' or '(a0)', then\n"
"  the used value will be read from the memory address pointed by it.\n"
"  If value is prefixed with '$', it's a hexadecimal, otherwise it's\n"
"  decimal value.\n"
"\n"
"  Example:\n"
"  	pc = $64543  &&  ($ff820).w & 3 = (a0)  &&  d0.l = 123\n\n");
}


/* ------------- breakpoint condition parsing, public API ------------ */

/**
 * Parse given DebugUI command for Dsp and act accordingly
 */
bool BreakCond_Command(const char *expression, bool bForDsp)
{
	unsigned int position;
	const char *end;
	
	if (!expression) {
		BreakCond_List(bForDsp);
		return true;
	}
	while (*expression == ' ') {
		expression++;
	}
	if (strncmp(expression, "help", 4) == 0) {
		BreakCond_Help();
		return true;
	}
	end = expression;
	while (isdigit(*end)) {
		end++;
	}
	if (end > expression && *end == '\0' &&
	    sscanf(expression, "%u", &position) == 1) {
		return BreakCond_Remove(position, bForDsp);
	}
	return BreakCond_Parse(expression, bForDsp);
}


/* ---------------------------- test code ---------------------------- */

/* Test building can be done in hatari/src/ with:
 * gcc -DTEST -I.. -Iincludes -Iuae-cpu -Ifalcon $(sdl-config --cflags) -O -Wall -g breakcond.c
 * 
 * TODO: move test stuff elsewhere after code works fully
 */
#if TEST

/* fake RAM */
Uint8 STRam[16*1024*1024];
Uint32 STRamEnd = 4*1024*1024;
/* fake registers */
static struct {
	Uint32 d0;
	Uint32 d1;
	Uint32 d2;
	Uint32 d3;
	Uint32 d4;
	Uint32 d5;
	Uint32 d6;
	Uint32 d7;
	Uint32 a0;
	Uint32 a1;
	Uint32 a2;
	Uint32 a3;
	Uint32 a4;
	Uint32 a5;
	Uint32 a6;
	Uint32 a7;
	Uint32 pc;
	Uint16 sr;
} CpuRegs;

/* emulate DSP register accessor */
size_t DSP_GetRegisterAddress(const char *regname, Uint32 **addr)
{
	const char *regnames[] = {
		"a0", "a1", "a2", "b0", "b1", "b2", "la", "lc",
		"m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7",
		"n0", "n1", "n2", "n3", "n4", "n5", "n6", "n7",
		"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
		"x0", "x1", "y0", "y1" /* PC, SR, SP, OMR, SSH, SSL */
	};
	static Uint32 registers[40];
	int i;
	for (i = 0; i < ARRAYSIZE(regnames); i++) {
		if (strcmp(regname, regnames[i]) == 0) {
			*addr = &(registers[i]);
			return 32;
		}
	}
	return 0;
}

/* emulate AUE register accessor */
Uint32 *DebugUI_GetCpuRegisterAddress(const char *regname)
{
	const char *regnames[] = {
		"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
		"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
		"pc" /* , "sr" */
	};
	int i;
	for (i = 0; i < ARRAYSIZE(regnames); i++) {
		if (strcmp(regname, regnames[i]) == 0) {
			return (&CpuRegs.d0)+i;
		}
	}
	return NULL;
}


int main(int argc, const char *argv[])
{
	const char *should_fail[] = {
		/* syntax & register name errors */
		"",
		" = ",
		" a0 d0 ",
		"gggg=a0",
		"=a=b=",
		"a0=d0=20",
		"a0=d || 0=20",
		"a0=d & 0=20",
		".w&3=2",
		"foo().w=bar()",
		"(a0.w=d0.l)",
		"(a0&3)=20",
		"20 = (a0.w)",
		"()&=d0",
		"d0=().w",
		"255 & 3 = (d0) & && 2 = 2",
		/* size and mask mismatches with numbers */
		"d0.w = $ffff0",
		"a0.b & 3 < 100",
		/* more than BC_MAX_CONDITIONS_PER_BREAKPOINT conditions */
		"1=1 && 2=2 && 3=3 && 4=4 && 5=5",
		NULL
	};
	const char *should_pass[] = {
		" ($200).w > 200 ",
		" ($200).w < 200 ",
		" (200).w = $200 ",
		" (200).w ! $200 ",
		"a0>d0",
		"a0<d0",
		"d0=d1",
		"d0!d1",
		"(a0)=(d0)",
		"(d0).w=(a0).b",
		"(a0).w&3=(d0)&&d0=1",
		" ( a 0 ) . w  &  1 = ( d 0 ) & 1 &&  d 0 = 3 ",
		"a0=1 && (d0)&2=(a0).w && ($00ff00).w&1=1",
		NULL
	};
	const char *test;
	bool use_dsp = false;
	int i, count, tests = 0, errors = 0;

	/* first automated tests... */
	
	fprintf(stderr, "\nShould FAIL:\n");
	for (i = 0; (test = should_fail[i]); i++) {
		fprintf(stderr, "-----------------\n- parsing '%s'\n", test);
		if (BreakCond_Command(test, use_dsp)) {
			fprintf(stderr, "***ERROR***: should have failed\n");
			errors++;
		}
	}
	tests += i;
	fprintf(stderr, "-----------------\n\n");
	BreakCond_List(use_dsp);
	
	fprintf(stderr, "\nShould PASS:\n");
	for (i = 0; (test = should_pass[i]); i++) {
		fprintf(stderr, "-----------------\n- parsing '%s'\n", test);
		if (!BreakCond_Command(test, use_dsp)) {
			fprintf(stderr, "***ERROR***: should have passed\n");
			errors++;
		}
	}
	tests += i;
	fprintf(stderr, "-----------------\n\n");
	BreakCond_List(use_dsp);
	fprintf(stderr, "\n");

	/* fail indirect equality checks with zerod regs */
	memset(STRam, 0, sizeof(STRam));
	STRam[0] = 1;
	/* 200<($200) fail */
	STRam[0x200] = 100;
	STRam[200] = 0x20;
	/* d0=d1 pass */
	CpuRegs.d0 = 4;
	CpuRegs.d1 = 4;
	CpuRegs.a0 = 8;
	while ((i = BreakCond_MatchCpu())) {
		fprintf(stderr, "Removing matching breakpoint.\n");
		BreakCond_Remove(i, use_dsp);
	}

	/* try removing everything from alternate ends */
	while ((count = BreakCond_BreakPointCount(use_dsp))) {
		BreakCond_Remove(count, use_dsp);
		BreakCond_Remove(1, use_dsp);
	}
	BreakCond_List(use_dsp);
	fprintf(stderr, "-----------------\n");

	/* ...last parse cmd line args */
	if (argc > 1) {
		fprintf(stderr, "\nCommand line breakpoints:\n");
		for (argv++; --argc > 0; argv++) {
			fprintf(stderr, "-----------------\n- parsing '%s'\n", *argv);
			BreakCond_Command(*argv, use_dsp);
		}
		fprintf(stderr, "-----------------\n\n");
		BreakCond_List(use_dsp);

		if (BreakCond_MatchCpu() || BreakCond_MatchDsp()) {
			fprintf(stderr, "-> CPU/DSP breakpoints matched\n\n");
		}
	}
	if (errors) {
		fprintf(stderr, "\n***Detected %d ERRORs in %d automated tests!***\n\n",
			errors, tests);
	}
	return 0;
}
#endif
