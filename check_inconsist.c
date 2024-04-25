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
#include <unistd.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FIELDS 5

static int my_id;
static char data_file[100];
static char output_file[100];

static inline void prefix() {
	printf("%s:%d %s() \n", get_filename(), get_lineno(), get_function());
}

static int not_protected_macro(char* macro) {
	const char* protected_macro_list[] = {
		"READ_ONCE",
		"WRITE_ONCE",
		"xchg",
		"atomic"
	};
	for (int i = 0; i < sizeof(protected_macro_list) / sizeof(protected_macro_list[0]); ++i)
		if (strstr(macro, protected_macro_list[i]) != NULL)
			return 0;
	return 1;
}

static char* string_in_file(const char *filename, const char *file_name_to_find, const char *func_name_to_find,
 const char *struct_type_to_find, const char *field_name_to_find) {
    FILE *file;
    char line[MAX_LINE_LENGTH];
    char *token;
    char *fields[MAX_FIELDS];

    // Open the file
    file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    // Read each line
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
        int field_count = 0;

        // Tokenize the line using commas as delimiters
        token = strtok(line, ",");
        while (token != NULL && field_count < MAX_FIELDS) {
            fields[field_count++] = token;
            token = strtok(NULL, ",");
        }

        // Process the fields
        if (field_count >= 5) {
            const char *file_name = fields[0];
            const char *func_name = fields[1];
            const char *stuct_type = fields[2];
            const char *field_name = fields[3];
			const char *line_num = fields[4];

			if (strcmp(file_name, file_name_to_find) == 0 &&
				strcmp(func_name, func_name_to_find) == 0 &&
				strcmp(stuct_type, struct_type_to_find) == 0 &&
				strcmp(field_name, field_name_to_find) == 0)
				return alloc_string(line_num);
        }
    }

    fclose(file);
    return NULL;
}

static bool is_assign_left(struct expression* expr) {
	struct expression *parent = expr_get_parent_expr(expr);
	char* expr_str = expr_to_str(expr);
	if (!parent)
		return 0;
	if (parent->type != EXPR_ASSIGNMENT)
		return 0;
	struct expression *expr_left = strip_expr(parent->left); 
	if (!expr_left)
		return 0;
	char* expr_left_str = expr_to_str(expr_left);
	if (expr_str && expr_left_str && strcmp(expr_str, expr_left_str) == 0)
		return 1;
	return 0;
}

void replaceSubstring(char *str, const char *oldSubstr, const char *newSubstr) {
    // Find the length of the substrings
    int oldLen = strlen(oldSubstr);
    int newLen = strlen(newSubstr);

    // Pointer for substring search
    char *pos = str;

    // Iterate over the string
    while ((pos = strstr(pos, oldSubstr)) != NULL) {
        // Replace the substring
        memmove(pos + newLen, pos + oldLen, strlen(pos + oldLen) + 1);
        memcpy(pos, newSubstr, newLen);
        pos += newLen;
    }
}

static void match_inconsist(struct expression *expr) {
	char* expr_str = expr_to_str(expr);
	struct expression *parent = expr_get_parent_expr(expr);
	// if (expr_str && parent) {
	// 	prefix();
	// 	char* parent_str = expr_to_str(parent);
	// 	if (parent_str)
	// 		printf("llk: %s %s parent string: %s\n", expr_str, expression_type_name(expr->type), parent_str);
	// }
	if (expr->type != EXPR_DEREF && expr->type != EXPR_SYMBOL)
		return;

	char* macro = get_macro_name(expr->pos);
	if (macro && !not_protected_macro(macro))
		return;

	if (parent && parent->type == EXPR_DEREF)
		return;

	if (parent && parent->type == EXPR_PREOP && parent->op == '&')
		return;

	if (expr->type == EXPR_DEREF && !is_held_lock() && !is_assign_left(expr)) {
		struct expression *expr_struct = expr->deref;
		struct ident *expr_field = expr->member;
		char* func_name = get_function();
		const char* file_name = get_filename();
		if (expr_struct && expr_field && func_name) {
			char* line_num = string_in_file(data_file, file_name, func_name, type_to_str(get_type(expr_struct)), expr_field->name);
			if (line_num) {
				// find inconsist
                FILE *fp = fopen(output_file, "a+");
                printf("generate output file %s\n", output_file);
                fprintf(fp, "%s,%s,%s,%d,%s",  get_filename(), func_name, expr_str, get_lineno(), line_num);
                fclose(fp);
			}
		}


	}
}

void check_inconsist(int id, char* file_name)
{
	my_id = id;

	replaceChar(file_name, '/', '_');
	strcpy(data_file, "/home/linke/Desktop/smatch/smatch_data/llk_data/");
	strcat(data_file, file_name);
	strcat(data_file, ".csv");
    strcpy(output_file, "/home/linke/Desktop/smatch/smatch_data/llk_output/result.csv");

	if (access(data_file, F_OK) == -1)
		return;

	add_hook(&match_inconsist, EXPR_HOOK);
}