//SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "wordtest.h"

static char *trim(char *str)
{
	char *end;

	while (isspace((unsigned char)*str))
		str++;

	end = str + strlen(str);

	while (end > str && isspace((unsigned char)end[-1]))
		end--;

	*end = 0;

	return str;
}

static void parse_title(char *comment, char **title)
{
	comment = trim(comment);

	if (strncmp(comment, "title:", 6))
		return;

	comment = trim(comment + 6);

	if (*comment)
		*title = strdup(comment);
}

ssize_t lesson_load(const char *path, char **title, struct word **words)
{
	FILE *f = fopen(path, "r");
	char *line = NULL;
	size_t line_size = 0;
	unsigned int lineno = 0;
	ssize_t cnt = 0;

	if (!f) {
		GP_WARN("Failed to open '%s': %s", path, strerror(errno));
		return -1;
	}

	while (getline(&line, &line_size, f) > 0) {
		char *str = line, *sep, *w0, *w1;

		lineno++;

		/* Skip UTF-8 BOM */
		if (lineno == 1 && !strncmp(str, "\xef\xbb\xbf", 3))
			str += 3;

		str = trim(str);

		if (!*str)
			continue;

		if (*str == '#') {
			if (title && !*title)
				parse_title(str + 1, title);
			continue;
		}

		sep = strchr(str, ';');
		if (!sep) {
			GP_WARN("%s:%u: Missing ';' separator", path, lineno);
			continue;
		}

		*sep = 0;
		w0 = trim(str);
		w1 = trim(sep + 1);

		if (!*w0 || !*w1) {
			GP_WARN("%s:%u: Empty word", path, lineno);
			continue;
		}

		cnt++;

		if (!words)
			continue;

		struct word word = {.w = {strdup(w0), strdup(w1)}};

		if (!word.w[0] || !word.w[1] || !GP_VEC_APPEND(*words, word)) {
			free(word.w[0]);
			free(word.w[1]);
			cnt = -1;
			break;
		}
	}

	free(line);
	fclose(f);

	return cnt;
}

struct word *words_pick(struct word *words, size_t cnt)
{
	size_t i, len = gp_vec_len(words);

	if (len <= cnt)
		return words;

	/* Partial Fisher-Yates, the picked words end up at the start */
	for (i = 0; i < cnt; i++) {
		size_t j = i + random() % (len - i);
		struct word tmp = words[i];

		words[i] = words[j];
		words[j] = tmp;
	}

	for (i = cnt; i < len; i++) {
		free(words[i].w[0]);
		free(words[i].w[1]);
	}

	return gp_vec_shrink(words, len - cnt);
}

void words_free(struct word *words)
{
	if (!words)
		return;

	GP_VEC_FOREACH(words, struct word, word) {
		free(word->w[0]);
		free(word->w[1]);
	}

	gp_vec_free(words);
}

static int is_space(uint32_t ch)
{
	return ch < 0x80 && isspace(ch);
}

/*
 * Copies src to dst with leading and trailing whitespaces removed, inner
 * whitespaces collapsed to a single space and letters lowercased.
 */
static void normalize(char *dst, size_t size, const char *src)
{
	size_t len = 0;
	int space = 0;
	uint32_t ch;

	while ((ch = gp_utf8_next(&src))) {
		if (is_space(ch)) {
			space = len > 0;
			continue;
		}

		ch = gp_utf_tolower(ch);

		if (len + space + gp_utf8_bytes(ch) + 1 > size)
			break;

		if (space)
			dst[len++] = ' ';

		space = 0;

		len += gp_to_utf8(ch, dst + len);
	}

	dst[len] = 0;
}

int answer_matches(const char *expected, const char *answer)
{
	char ans[256], exp[256], alt[256];
	char *saveptr, *tok;

	normalize(ans, sizeof(ans), answer);

	if (!ans[0])
		return 0;

	snprintf(exp, sizeof(exp), "%s", expected);

	for (tok = strtok_r(exp, ",", &saveptr); tok; tok = strtok_r(NULL, ",", &saveptr)) {
		normalize(alt, sizeof(alt), tok);

		if (!strcmp(alt, ans))
			return 1;
	}

	return 0;
}

void shuffle(unsigned int *arr, size_t len)
{
	size_t i;

	for (i = len; i > 1; i--) {
		size_t j = random() % i;
		GP_SWAP(arr[i-1], arr[j]);
	}
}
