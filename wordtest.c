//SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wordtest.h"

#define APP_NAME "wordtest"
#define SELECTION_CFG "selection.txt"

enum layouts {
	LAYOUT_MENU,
	LAYOUT_QUIZ,
	LAYOUT_EXAM,
	LAYOUT_RESULTS,
};

static struct lang_pair *pairs;
static int from_cmdline;

static size_t pair_sel;
static size_t course_sel;
/* 0 is all lessons, otherwise lesson index + 1 */
static size_t lesson_sel;
static enum direction dir_sel;
static char dir_labels[3][128];

const struct lang_pair *cur_pair;
struct word *words;

static gp_widget *layout_switch;
static gp_widget *course_choice;
static gp_widget *dir_choice;
static gp_widget *lessons;
static gp_widget *max_words;
static gp_widget *results_title;
static gp_widget *results_summary;
static gp_widget *results_missed;

static const unsigned int *missed;
static size_t missed_cnt;

gp_app_info app_info = {
	.name = APP_NAME,
	.desc = "A vocabulary test",
	.version = "1.0",
	.license = "GPL-2.0-or-later",
	.url = "http://github.com/gfxprim/wordtest",
	.authors = (gp_app_info_author []) {
		{.name = "Cyril Hrubis", .email = "metan@ucw.cz", .years = "2026"},
		{}
	}
};

static size_t pairs_cnt(void)
{
	return pairs ? gp_vec_len(pairs) : 0;
}

static struct course *cur_course(void)
{
	return &cur_pair->courses[course_sel];
}

unsigned int dir_from(void)
{
	switch (dir_sel) {
	case DIR_BACKWARD:
		return 1;
	case DIR_FORWARD:
		return 0;
	default:
		return random() & 1;
	}
}

void status_set(gp_widget *label, enum gp_widgets_color color, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	gp_widget_label_vprintf(label, fmt, ap);
	va_end(ap);

	gp_widget_label_colors_set(label, color, GP_WIDGETS_COL_BG);
}

void show_menu(void)
{
	gp_widget_layout_switch_layout(layout_switch, LAYOUT_MENU);
}

void show_results(const char *title, const char *summary,
                  const unsigned int *missed_words, size_t missed_words_cnt)
{
	missed = missed_words;
	missed_cnt = missed_words_cnt;

	gp_widget_label_set(results_title, title);
	gp_widget_label_set(results_summary, summary);
	gp_widget_table_off_set(results_missed, 0);
	gp_widget_layout_switch_layout(layout_switch, LAYOUT_RESULTS);
}

static int missed_seek_row(gp_widget *self, int op, unsigned int pos)
{
	gp_widget_table_priv *tbl_priv = gp_widget_table_priv_get(self);

	switch (op) {
	case GP_TABLE_ROW_RESET:
		tbl_priv->row_idx = 0;
	break;
	case GP_TABLE_ROW_ADVANCE:
		tbl_priv->row_idx += pos;
	break;
	case GP_TABLE_ROW_MAX:
		return missed_cnt;
	}

	return tbl_priv->row_idx < missed_cnt;
}

static int missed_get_cell(gp_widget *self, gp_widget_table_cell *cell, unsigned int col)
{
	gp_widget_table_priv *tbl_priv = gp_widget_table_priv_get(self);
	const struct word *word = &words[missed[tbl_priv->row_idx]];

	if (col > 1)
		return 0;

	/* Shown in the direction words are asked in by default */
	cell->text = word->w[!col];
	cell->tattr = 0;

	return 1;
}

const gp_widget_table_col_ops results_col_ops = {
	.seek_row = missed_seek_row,
	.get_cell = missed_get_cell,
	.col_map = {
		{.id = "word", .idx = 0},
		{.id = "translation", .idx = 1},
		{}
	}
};

int back_to_menu(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	show_menu();

	return 0;
}

static void lessons_reset(size_t sel)
{
	lesson_sel = sel;

	gp_widget_table_off_set(lessons, 0);
	gp_widget_table_sel_set(lessons, sel);
}

static void dir_labels_update(void)
{
	snprintf(dir_labels[DIR_BACKWARD], sizeof(dir_labels[0]), "%s -> %s",
	         cur_pair->lang[1], cur_pair->lang[0]);
	snprintf(dir_labels[DIR_FORWARD], sizeof(dir_labels[0]), "%s -> %s",
	         cur_pair->lang[0], cur_pair->lang[1]);
	snprintf(dir_labels[DIR_MIXED], sizeof(dir_labels[0]), "Mixed");
}

static void pair_select(size_t pair, size_t course, size_t lesson)
{
	pair_sel = pair;
	cur_pair = &pairs[pair];
	course_sel = course;

	dir_labels_update();

	gp_widget_choice_refresh(course_choice);
	gp_widget_choice_refresh(dir_choice);

	lessons_reset(lesson);
}

static const char *pair_get_choice(gp_widget *self, size_t idx)
{
	(void)self;

	/* Spinbutton asks for the selected choice even if there are none */
	if (idx >= pairs_cnt())
		return "";

	return pairs[idx].label;
}

static size_t pair_get(gp_widget *self, enum gp_widget_choice_op op)
{
	(void)self;

	switch (op) {
	case GP_WIDGET_CHOICE_OP_SEL:
		return pair_sel;
	case GP_WIDGET_CHOICE_OP_CNT:
		return pairs_cnt();
	}

	return 0;
}

static void pair_set(gp_widget *self, size_t idx)
{
	(void)self;

	if (idx < pairs_cnt())
		pair_select(idx, 0, 0);
}

const gp_widget_choice_desc pair_desc = {
	.ops = &(gp_widget_choice_ops) {
		.get_choice = pair_get_choice,
		.get = pair_get,
		.set = pair_set,
	}
};

static size_t courses_cnt(void)
{
	return cur_pair ? gp_vec_len(cur_pair->courses) : 0;
}

static const char *course_get_choice(gp_widget *self, size_t idx)
{
	(void)self;

	/* Spinbutton asks for the selected choice even if there are none */
	if (idx >= courses_cnt())
		return "";

	return cur_pair->courses[idx].name;
}

static size_t course_get(gp_widget *self, enum gp_widget_choice_op op)
{
	(void)self;

	switch (op) {
	case GP_WIDGET_CHOICE_OP_SEL:
		return course_sel;
	case GP_WIDGET_CHOICE_OP_CNT:
		return courses_cnt();
	}

	return 0;
}

static void course_set(gp_widget *self, size_t idx)
{
	(void)self;

	if (idx >= courses_cnt())
		return;

	course_sel = idx;
	lessons_reset(0);
}

const gp_widget_choice_desc course_desc = {
	.ops = &(gp_widget_choice_ops) {
		.get_choice = course_get_choice,
		.get = course_get,
		.set = course_set,
	}
};

static const char *dir_get_choice(gp_widget *self, size_t idx)
{
	(void)self;

	if (idx >= GP_ARRAY_SIZE(dir_labels))
		return NULL;

	return dir_labels[idx];
}

static size_t dir_get(gp_widget *self, enum gp_widget_choice_op op)
{
	(void)self;

	switch (op) {
	case GP_WIDGET_CHOICE_OP_SEL:
		return dir_sel;
	case GP_WIDGET_CHOICE_OP_CNT:
		return GP_ARRAY_SIZE(dir_labels);
	}

	return 0;
}

static void dir_set(gp_widget *self, size_t idx)
{
	(void)self;

	dir_sel = idx;
}

const gp_widget_choice_desc dir_desc = {
	.ops = &(gp_widget_choice_ops) {
		.get_choice = dir_get_choice,
		.get = dir_get,
		.set = dir_set,
	}
};

static size_t lessons_cnt(void)
{
	if (!cur_pair)
		return 0;

	return gp_vec_len(cur_course()->lessons);
}

static int lessons_seek_row(gp_widget *self, int op, unsigned int pos)
{
	gp_widget_table_priv *tbl_priv = gp_widget_table_priv_get(self);
	/* The first row is "All lessons" or "No courses found" */
	size_t rows = lessons_cnt() + 1;

	switch (op) {
	case GP_TABLE_ROW_RESET:
		tbl_priv->row_idx = 0;
	break;
	case GP_TABLE_ROW_ADVANCE:
		tbl_priv->row_idx += pos;
	break;
	case GP_TABLE_ROW_MAX:
		return rows;
	}

	return tbl_priv->row_idx < rows;
}

static int lessons_get_cell(gp_widget *self, gp_widget_table_cell *cell, unsigned int col)
{
	gp_widget_table_priv *tbl_priv = gp_widget_table_priv_get(self);
	const struct course *course = cur_pair ? cur_course() : NULL;
	size_t row = tbl_priv->row_idx;
	static char buf[32];

	if (!cur_pair) {
		cell->text = col ? "" : "No courses found!";
		cell->tattr = GP_TATTR_BOLD;
		return 1;
	}

	switch (col) {
	case 0:
		cell->text = row ? course->lessons[row-1].name : "All lessons";
		cell->tattr = row ? 0 : GP_TATTR_BOLD;
	break;
	case 1:
		snprintf(buf, sizeof(buf), "%zu",
		         row ? course->lessons[row-1].word_cnt : course->word_cnt);
		cell->text = buf;
		cell->tattr = GP_TATTR_RIGHT;
	break;
	default:
		return 0;
	}

	return 1;
}

static int lessons_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (ev->sub_type == GP_WIDGET_TABLE_SELECT)
		lesson_sel = gp_widget_table_sel_get(ev->self);

	return 0;
}

const gp_widget_table_col_ops lessons_col_ops = {
	.seek_row = lessons_seek_row,
	.get_cell = lessons_get_cell,
	.on_event = lessons_event,
	.col_map = {
		{.id = "name", .idx = 0},
		{.id = "words", .idx = 1},
		{}
	}
};

static void selection_save(void)
{
	const struct course *course = cur_course();

	if (from_cmdline)
		return;

	gp_app_cfg_printf(APP_NAME, SELECTION_CFG, "%s\n%s\n%s\n%i\n%lli\n",
	                  cur_pair->id, course->id,
	                  lesson_sel ? course->lessons[lesson_sel-1].file : "*",
	                  dir_sel, (long long)gp_widget_int_val_get(max_words));
}

static void selection_restore(void)
{
	char pair_id[128], course_id[128], lesson_file[256];
	size_t pair = 0, course = 0, lesson = 0, i;
	int dir, ret;
	long long max;

	ret = gp_app_cfg_scanf(APP_NAME, SELECTION_CFG, "%127[^\n]\n%127[^\n]\n%255[^\n]\n%i\n%lli",
	                       pair_id, course_id, lesson_file, &dir, &max);
	if (ret < 4) {
		pair_select(0, 0, 0);
		return;
	}

	/* Files saved before the words per session were added have 4 lines */
	if (ret == 5 && max >= gp_widget_int_min_get(max_words) &&
	    max <= gp_widget_int_max_get(max_words))
		gp_widget_int_val_set(max_words, max);

	if (dir >= DIR_BACKWARD && dir <= DIR_MIXED)
		dir_sel = dir;

	for (i = 0; i < pairs_cnt(); i++) {
		if (!strcmp(pairs[i].id, pair_id)) {
			pair = i;
			break;
		}
	}

	const struct lang_pair *p = &pairs[pair];

	for (i = 0; i < gp_vec_len(p->courses); i++) {
		if (!strcmp(p->courses[i].id, course_id)) {
			course = i;
			break;
		}
	}

	const struct course *c = &p->courses[course];

	for (i = 0; i < gp_vec_len(c->lessons); i++) {
		if (!strcmp(c->lessons[i].file, lesson_file)) {
			lesson = i + 1;
			break;
		}
	}

	pair_select(pair, course, lesson);
}

static int load_words(void)
{
	const struct course *course;
	size_t i;

	words_free(words);
	words = NULL;

	if (!cur_pair)
		return 1;

	course = cur_course();

	for (i = 0; i < gp_vec_len(course->lessons); i++) {
		if (lesson_sel && lesson_sel != i + 1)
			continue;

		if (lesson_load(course->lessons[i].path, NULL, &words) < 0) {
			gp_dialog_msg_printf_run(GP_DIALOG_MSG_ERR, "Error",
			                         "Failed to load '%s'",
			                         course->lessons[i].path);
			return 1;
		}
	}

	if (!words || !gp_vec_len(words)) {
		gp_dialog_msg_run(GP_DIALOG_MSG_ERR, "Error", "No words to test");
		return 1;
	}

	words = words_pick(words, gp_widget_int_val_get(max_words));

	selection_save();

	return 0;
}

int start_quiz(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (load_words())
		return 0;

	gp_widget_layout_switch_layout(layout_switch, LAYOUT_QUIZ);
	quiz_start();

	return 0;
}

int start_exam(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (load_words())
		return 0;

	gp_widget_layout_switch_layout(layout_switch, LAYOUT_EXAM);
	exam_start();

	return 0;
}

static void no_courses(gp_htable *uids)
{
	const char *const *paths = courses_search_paths();

	gp_widget_disable(gp_widget_by_cuid(uids, "pair", GP_WIDGET_CLASS_CHOICE));
	gp_widget_disable(course_choice);
	gp_widget_disable(dir_choice);
	gp_widget_disable(lessons);
	gp_widget_disable(max_words);
	gp_widget_disable(gp_widget_by_uid(uids, "quiz_btn", GP_WIDGET_BUTTON));
	gp_widget_disable(gp_widget_by_uid(uids, "exam_btn", GP_WIDGET_BUTTON));

	fprintf(stderr, "No courses found, searched in:\n");

	for (; *paths; paths++)
		fprintf(stderr, "  %s\n", *paths);
}

int main(int argc, char *argv[])
{
	gp_widget *layout;
	gp_htable *uids;

	srandom(time(NULL));

	gp_widgets_getopt(&argc, &argv);

	if (argv[0]) {
		pairs = courses_from_file(argv[0]);
		if (!pairs) {
			fprintf(stderr, "Failed to load lesson '%s'\n", argv[0]);
			return 1;
		}
		from_cmdline = 1;
	} else {
		pairs = courses_discover();
	}

	layout = gp_app_layout_load(APP_NAME, &uids);
	if (!layout)
		return 1;

	layout_switch = gp_widget_by_uid(uids, "layout_switch", GP_WIDGET_LAYOUT_SWITCH);
	course_choice = gp_widget_by_cuid(uids, "course", GP_WIDGET_CLASS_CHOICE);
	dir_choice = gp_widget_by_cuid(uids, "dir", GP_WIDGET_CLASS_CHOICE);
	lessons = gp_widget_by_uid(uids, "lessons", GP_WIDGET_TABLE);
	max_words = gp_widget_by_cuid(uids, "max_words", GP_WIDGET_CLASS_INT);
	results_title = gp_widget_by_uid(uids, "results_title", GP_WIDGET_LABEL);
	results_summary = gp_widget_by_uid(uids, "results_summary", GP_WIDGET_LABEL);
	results_missed = gp_widget_by_uid(uids, "results_missed", GP_WIDGET_TABLE);

	quiz_init(uids);
	exam_init(uids);

	if (!pairs)
		no_courses(uids);
	else if (from_cmdline)
		pair_select(0, 0, 1);
	else
		selection_restore();

	gp_htable_free(uids);

	gp_widgets_main_loop(layout, NULL, 0, NULL);

	return 0;
}
