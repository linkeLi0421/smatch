/*
 * Copyright (C) 2024 Linke Li.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/copyleft/gpl.txt
 */

#include "smatch.h"
#include "smatch_slist.h"
#include <stdio.h>
#include <stdlib.h>
#include "smatch_extra.h"
#include <string.h>

static int my_id;
static char data_file[100];

static const char *expression_type_name(enum expression_type type)
{
	static const char *expression_type_name[] = {
		[EXPR_VALUE] = "EXPR_VALUE",
		[EXPR_STRING] = "EXPR_STRING",
		[EXPR_SYMBOL] = "EXPR_SYMBOL",
		[EXPR_TYPE] = "EXPR_TYPE",
		[EXPR_BINOP] = "EXPR_BINOP",
		[EXPR_ASSIGNMENT] = "EXPR_ASSIGNMENT",
		[EXPR_LOGICAL] = "EXPR_LOGICAL",
		[EXPR_DEREF] = "EXPR_DEREF",
		[EXPR_PREOP] = "EXPR_PREOP",
		[EXPR_POSTOP] = "EXPR_POSTOP",
		[EXPR_CAST] = "EXPR_CAST",
		[EXPR_FORCE_CAST] = "EXPR_FORCE_CAST",
		[EXPR_IMPLIED_CAST] = "EXPR_IMPLIED_CAST",
		[EXPR_SIZEOF] = "EXPR_SIZEOF",
		[EXPR_ALIGNOF] = "EXPR_ALIGNOF",
		[EXPR_PTRSIZEOF] = "EXPR_PTRSIZEOF",
		[EXPR_CONDITIONAL] = "EXPR_CONDITIONAL",
		[EXPR_SELECT] = "EXPR_SELECT",
		[EXPR_STATEMENT] = "EXPR_STATEMENT",
		[EXPR_CALL] = "EXPR_CALL",
		[EXPR_COMMA] = "EXPR_COMMA",
		[EXPR_COMPARE] = "EXPR_COMPARE",
		[EXPR_LABEL] = "EXPR_LABEL",
		[EXPR_INITIALIZER] = "EXPR_INITIALIZER",
		[EXPR_IDENTIFIER] = "EXPR_IDENTIFIER",
		[EXPR_INDEX] = "EXPR_INDEX",
		[EXPR_POS] = "EXPR_POS",
		[EXPR_FVALUE] = "EXPR_FVALUE",
		[EXPR_SLICE] = "EXPR_SLICE",
		[EXPR_OFFSETOF] = "EXPR_OFFSETOF",
	};
	return expression_type_name[type] ?: "UNKNOWN_EXPRESSION_TYPE";
}

static inline void prefix() {
	printf("%s:%d %s() \n", get_filename(), get_lineno(), get_function());
}

char* string_in_file(const char *filename, const char *string_to_find) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "fopen() failed.\n");
        return 0;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strstr(buffer, string_to_find) != NULL) {
            fclose(file);
            return alloc_string(buffer);
        }
    }

    fclose(file);
    return NULL;
}

int is_protected_macro(char* macro) {
	return strcmp(macro, "READ_ONCE") && strcmp(macro, "WRITE_ONCE");
}

// EXPR_SYMBOL + EXPR_DEREF
static void match_barrier(struct expression *expr)
{
	char *macro;
	char *expr_str;
	struct expression *parent;

	macro = get_macro_name(expr->pos);
	if (!macro)
		return;
	if (strcmp(macro, "READ_ONCE") != 0)
		return;
	if (expr->type != EXPR_DEREF && expr->type != EXPR_SYMBOL)
		return;
	
	expr_str = expr_to_str(expr);
	parent = expr_get_parent_expr(expr);
	if (expr_str && parent) {
		prefix();
		char* parent_str = expr_to_str(parent);
		if (parent_str)
			printf("llk: %s %s parent string: %s\n", expr_str, expression_type_name(expr->type), parent_str);
	}

	if (expr->type == EXPR_DEREF) {
		struct expression *expr_struct = expr->deref;
		struct ident *expr_field = expr->member;
		char *expr_struct_str = expr_to_str(expr_struct);
		printf("struct: type of '%s' is: '%s'\n", expr_struct_str, type_to_str(get_type(expr_struct)));
		printf("field: %s\n", expr_field->name);
		FILE *fp = fopen(data_file, "a+");
		if (fp == NULL) {
			fprintf(stderr, "fopen() failed.\n");
			exit(EXIT_FAILURE);
    	}
		fprintf(fp, "%s,%s,%s,%d\n", get_function(), type_to_str(get_type(expr_struct)), expr_field->name, get_lineno());
		fclose(fp);
	}
	
}

static void match_inconsist(struct expression *expr) {
	if (expr->type != EXPR_DEREF && expr->type != EXPR_SYMBOL)
		return;

	char* macro = get_macro_name(expr->pos);
	if (macro && !is_protected_macro(macro))
		return;

	if (expr->type == EXPR_DEREF) {
		struct expression *expr_struct = expr->deref;
		struct ident *expr_field = expr->member;
		char str_to_find[1024] = "";
		char* func_name = get_function();
		if (expr_struct && expr_field && func_name) {
			strcat(str_to_find, func_name);
			strcat(str_to_find, ",");
			strcat(str_to_find, type_to_str(get_type(expr_struct)));
			strcat(str_to_find, ",");
			strcat(str_to_find, expr_field->name);
			char* buffer_match = string_in_file(data_file, str_to_find);
			if (buffer_match) {
				// find inconsist
				printf("find inconsist: %s,%d\nwith: %s\n", str_to_find, get_lineno(), buffer_match);
			}
		}


	}
}

void replaceChar(char *str, char oldChar, char newChar) {
    while (*str != '\0') {
        if (*str == oldChar) {
            *str = newChar;
        }
        str++;
    }
}

void check_cb(int id, char* file_name)
{
	my_id = id;

	replaceChar(file_name, '/', '_');
	strcpy(data_file, "/home/linke/Desktop/smatch/smatch_data/llk_data/");
	strcat(data_file, file_name);
	strcat(data_file, ".csv");
	printf("data_file: %s\n", data_file);

	add_hook(&match_barrier, EXPR_HOOK);
	// add_hook(&match_nonconsist, EXPR_HOOK);
}