//SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include "wordtest.h"

static const struct {
	const char *code;
	const char *name;
} lang_names[] = {
	{"cs", "Czech"},
	{"cz", "Czech"},
	{"de", "German"},
	{"en", "English"},
	{"es", "Spanish"},
	{"fr", "French"},
	{"it", "Italian"},
	{"nl", "Dutch"},
	{"pl", "Polish"},
	{"pt", "Portuguese"},
	{"ru", "Russian"},
	{"sk", "Slovak"},
	{"uk", "Ukrainian"},
};

static const char *lang_name(char *code)
{
	size_t i;

	for (i = 0; i < GP_ARRAY_SIZE(lang_names); i++) {
		if (!strcmp(lang_names[i].code, code))
			return lang_names[i].name;
	}

	return code;
}

static int is_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st))
		return 0;

	return S_ISDIR(st.st_mode);
}

static int is_file(const char *path)
{
	struct stat st;

	if (stat(path, &st))
		return 0;

	return S_ISREG(st.st_mode);
}

static int dot_filter(const struct dirent *ent)
{
	return ent->d_name[0] != '.';
}

static char *path_join(const char *dir, const char *name)
{
	char *ret;

	if (asprintf(&ret, "%s/%s", dir, name) < 0)
		return NULL;

	return ret;
}

/*
 * Creates a pair from the "xx-yy" directory name, returns non-zero if the
 * name is not a language pair.
 */
static int pair_init(struct lang_pair *pair, const char *id)
{
	const char *sep = strchr(id, '-');
	char *codes;

	if (!sep || sep == id || !sep[1] || strchr(sep + 1, '-'))
		return 1;

	pair->id = strdup(id);
	codes = strdup(id);

	if (!pair->id || !codes)
		goto err;

	codes[sep - id] = 0;

	pair->lang[0] = lang_name(codes);
	pair->lang[1] = lang_name(codes + (sep - id) + 1);

	if (asprintf(&pair->label, "%s - %s", pair->lang[0], pair->lang[1]) < 0)
		goto err;

	pair->courses = NULL;

	return 0;
err:
	free(pair->id);
	free(codes);
	return 1;
}

/*
 * Makes a lesson name from a file name, i.e. "01-family_tree.txt" -> "Family tree".
 */
static char *lesson_name(const char *fname)
{
	const char *start = fname, *end;
	char *ret, *c;

	while (isdigit((unsigned char)*start))
		start++;

	if (start != fname) {
		while (*start == '-' || *start == '_' || *start == ' ')
			start++;
	}

	end = strrchr(start, '.');

	if (start == end)
		start = fname;

	ret = strndup(start, end - start);
	if (!ret)
		return NULL;

	for (c = ret; *c; c++) {
		if (*c == '_' || *c == '-')
			*c = ' ';
	}

	ret[0] = toupper(ret[0]);

	return ret;
}

static int is_lesson_file(const char *fname)
{
	size_t len = strlen(fname);

	if (len <= 4 || strcmp(fname + len - 4, ".txt"))
		return 0;

	return strcmp(fname, "course.txt");
}

static int lesson_init(struct lesson *lesson, const char *dir, const char *fname)
{
	char *title = NULL;
	ssize_t cnt;

	lesson->path = path_join(dir, fname);
	if (!lesson->path)
		return 1;

	cnt = lesson_load(lesson->path, &title, NULL);
	if (cnt <= 0) {
		if (!cnt)
			GP_WARN("No words in '%s'", lesson->path);
		goto err;
	}

	lesson->word_cnt = cnt;
	lesson->file = strdup(fname);
	lesson->name = title ? title : lesson_name(fname);

	if (!lesson->file || !lesson->name) {
		free(lesson->file);
		free(lesson->name);
		goto err;
	}

	return 0;
err:
	free(lesson->path);
	return 1;
}

static char *course_name(const char *dir)
{
	char *path = path_join(dir, "course.txt");
	char *line = NULL, *ret = NULL;
	size_t size = 0;
	FILE *f;

	if (!path)
		return NULL;

	f = fopen(path, "r");
	free(path);

	if (!f)
		return NULL;

	while (getline(&line, &size, f) > 0) {
		char *str = line;

		line[strcspn(line, "\r\n")] = 0;

		while (isspace((unsigned char)*str))
			str++;

		if (*str && *str != '#') {
			ret = strdup(str);
			break;
		}
	}

	free(line);
	fclose(f);

	return ret;
}

/*
 * Loads course lessons, returns non-zero if there are none.
 */
static int course_init(struct course *course, const char *dir, const char *id)
{
	struct dirent **ents;
	int i, cnt;

	course->lessons = NULL;
	course->word_cnt = 0;

	cnt = scandir(dir, &ents, dot_filter, alphasort);
	if (cnt < 0)
		return 1;

	for (i = 0; i < cnt; i++) {
		const char *fname = ents[i]->d_name;
		struct lesson lesson;
		char *path;

		if (!is_lesson_file(fname))
			goto next;

		path = path_join(dir, fname);
		if (!path || !is_file(path)) {
			free(path);
			goto next;
		}
		free(path);

		if (lesson_init(&lesson, dir, fname))
			goto next;

		if (GP_VEC_APPEND(course->lessons, lesson))
			course->word_cnt += lesson.word_cnt;
next:
		free(ents[i]);
	}

	free(ents);

	if (!course->lessons)
		return 1;

	course->id = strdup(id);
	course->name = course_name(dir);

	if (!course->name)
		course->name = strdup(id);

	return 0;
}

static struct lang_pair *pair_find(struct lang_pair *pairs, const char *id)
{
	if (!pairs)
		return NULL;

	GP_VEC_FOREACH(pairs, struct lang_pair, pair) {
		if (!strcmp(pair->id, id))
			return pair;
	}

	return NULL;
}

static int course_exists(struct lang_pair *pair, const char *id)
{
	if (!pair->courses)
		return 0;

	GP_VEC_FOREACH(pair->courses, struct course, course) {
		if (!strcmp(course->id, id))
			return 1;
	}

	return 0;
}

static void scan_pair(struct lang_pair *pair, const char *dir)
{
	struct dirent **ents;
	int i, cnt;

	cnt = scandir(dir, &ents, dot_filter, alphasort);
	if (cnt < 0)
		return;

	for (i = 0; i < cnt; i++) {
		const char *id = ents[i]->d_name;
		struct course course;
		char *path;

		if (course_exists(pair, id))
			goto next;

		path = path_join(dir, id);

		if (path && is_dir(path) && !course_init(&course, path, id))
			GP_VEC_APPEND(pair->courses, course);

		free(path);
next:
		free(ents[i]);
	}

	free(ents);
}

static struct lang_pair *scan_base(struct lang_pair *pairs, const char *base)
{
	struct dirent **ents;
	int i, cnt;

	cnt = scandir(base, &ents, dot_filter, alphasort);
	if (cnt < 0)
		return pairs;

	GP_DEBUG(1, "Scanning '%s' for courses", base);

	for (i = 0; i < cnt; i++) {
		const char *id = ents[i]->d_name;
		struct lang_pair *pair, new_pair;
		char *path = path_join(base, id);

		if (!path || !is_dir(path))
			goto next;

		pair = pair_find(pairs, id);
		if (!pair) {
			if (pair_init(&new_pair, id))
				goto next;

			if (!GP_VEC_APPEND(pairs, new_pair))
				goto next;

			pair = &pairs[gp_vec_len(pairs) - 1];
		}

		scan_pair(pair, path);
next:
		free(path);
		free(ents[i]);
	}

	free(ents);

	return pairs;
}

static int course_cmp(const void *a, const void *b)
{
	const struct course *ca = a, *cb = b;

	return strcmp(ca->name, cb->name);
}

static int pair_cmp(const void *a, const void *b)
{
	const struct lang_pair *pa = a, *pb = b;

	return strcmp(pa->label, pb->label);
}

static char *search_paths[5];

const char *const *courses_search_paths(void)
{
	const char *data_home = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");
	size_t i = 0;

	if (search_paths[0])
		return (const char *const *)search_paths;

	search_paths[i++] = "./courses";

	if (data_home && data_home[0]) {
		if (asprintf(&search_paths[i], "%s/wordtest", data_home) >= 0)
			i++;
	} else if (home) {
		if (asprintf(&search_paths[i], "%s/.local/share/wordtest", home) >= 0)
			i++;
	}

	search_paths[i++] = "/usr/local/share/wordtest";
	search_paths[i++] = "/usr/share/wordtest";

	return (const char *const *)search_paths;
}

struct lang_pair *courses_discover(void)
{
	const char *const *paths = courses_search_paths();
	struct lang_pair *pairs = NULL;
	size_t i, j;

	for (i = 0; paths[i]; i++)
		pairs = scan_base(pairs, paths[i]);

	if (!pairs)
		return NULL;

	/* Drop pairs without any courses */
	for (i = 0, j = 0; i < gp_vec_len(pairs); i++) {
		if (pairs[i].courses)
			pairs[j++] = pairs[i];
	}

	if (!j) {
		gp_vec_free(pairs);
		return NULL;
	}

	pairs = gp_vec_shrink(pairs, gp_vec_len(pairs) - j);

	qsort(pairs, gp_vec_len(pairs), sizeof(*pairs), pair_cmp);

	GP_VEC_FOREACH(pairs, struct lang_pair, pair)
		qsort(pair->courses, gp_vec_len(pair->courses), sizeof(struct course), course_cmp);

	return pairs;
}

struct lang_pair *courses_from_file(const char *path)
{
	char *abs_path = realpath(path, NULL);
	char *dir, *course_dir, *pair_dir, *tmp;
	struct lang_pair pair, *pairs = NULL;
	struct course course = {};
	struct lesson lesson;

	if (!abs_path) {
		GP_WARN("Failed to resolve '%s': %s", path, strerror(errno));
		return NULL;
	}

	dir = strdup(abs_path);
	tmp = strdup(abs_path);
	if (!dir || !tmp)
		goto err;

	course_dir = dirname(dir);
	pair_dir = strdup(course_dir);
	if (!pair_dir)
		goto err;

	if (pair_init(&pair, basename(dirname(pair_dir)))) {
		pair.id = strdup("unknown");
		pair.lang[0] = "First language";
		pair.lang[1] = "Second language";
		pair.label = strdup("Unknown languages");
		pair.courses = NULL;
	}

	free(pair_dir);

	if (lesson_init(&lesson, course_dir, basename(tmp)))
		goto err;

	course.id = strdup(basename(course_dir));
	course.name = strdup(course.id);
	course.word_cnt = lesson.word_cnt;

	if (!GP_VEC_APPEND(course.lessons, lesson) ||
	    !GP_VEC_APPEND(pair.courses, course) ||
	    !GP_VEC_APPEND(pairs, pair))
		goto err;

	free(abs_path);
	free(dir);
	free(tmp);

	return pairs;
err:
	free(abs_path);
	free(dir);
	free(tmp);
	return NULL;
}
