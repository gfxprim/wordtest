//SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wordtest.h"

#define QUIZ_ROWS 6

static gp_widget *lang_label[2];
static gp_widget *status;
static gp_widget *progress;

/* Push buttons, the first grid row is the header */
static gp_widget *btns[2][QUIZ_ROWS];

/* Indexes into words for each button */
static unsigned int btn_word[2][QUIZ_ROWS];

/* Selected button row in each column or -1 */
static int sel[2] = {-1, -1};

/* Shuffled word indexes */
static unsigned int *order;
static size_t order_len;
static size_t order_pos;

static unsigned int round_size;
static unsigned int round_matched;
static unsigned int matched;
static unsigned int mistakes;

/* Words that were part of a wrong pair */
static unsigned int *missed;
static size_t missed_cnt;

/* Languages shown in the left and right column */
static unsigned int from, to;

static void progress_update(void)
{
	gp_widget_label_printf(progress, "Matched %u / %zu, mistakes: %u",
	                       matched, order_len, mistakes);
}

static void finish(void)
{
	char msg[128];

	snprintf(msg, sizeof(msg), "All %zu pairs matched with %u mistake%s",
	         order_len, mistakes, mistakes == 1 ? "" : "s");

	show_results("Quiz finished", msg, missed, missed_cnt);
}

static void next_round(void)
{
	unsigned int perm[QUIZ_ROWS];
	unsigned int i, col;

	if (order_pos >= order_len) {
		finish();
		return;
	}

	round_size = GP_MIN((size_t)QUIZ_ROWS, order_len - order_pos);
	round_matched = 0;
	sel[0] = sel[1] = -1;

	for (i = 0; i < round_size; i++)
		perm[i] = i;

	shuffle(perm, round_size);

	for (i = 0; i < round_size; i++) {
		unsigned int word = order[order_pos + i];

		btn_word[0][i] = word;
		btn_word[1][perm[i]] = word;
	}

	order_pos += round_size;

	for (i = 0; i < QUIZ_ROWS; i++) {
		for (col = 0; col < 2; col++) {
			gp_widget *btn = btns[col][i];

			gp_widget_bool_set(btn, false);

			/* Buttons not used in a shorter last round are cleared and disabled */
			if (i >= round_size) {
				gp_widget_push_button_label_set(btn, NULL);
				gp_widget_disable(btn);
				continue;
			}

			gp_widget_push_button_label_set(btn, words[btn_word[col][i]].w[col ? to : from]);
			gp_widget_enable(btn);
		}
	}

	gp_widget_focus_set(btns[0][0]);
}

static void missed_add(unsigned int word)
{
	size_t i;

	for (i = 0; i < missed_cnt; i++) {
		if (missed[i] == word)
			return;
	}

	missed[missed_cnt++] = word;
}

static void check_pair(void)
{
	gp_widget *left = btns[0][sel[0]];
	gp_widget *right = btns[1][sel[1]];
	unsigned int lidx = btn_word[0][sel[0]];
	unsigned int ridx = btn_word[1][sel[1]];
	const struct word *lw = &words[lidx];
	const struct word *rw = &words[ridx];

	/* Compare strings so that words with the same meaning match as well */
	int match = !strcmp(lw->w[to], rw->w[to]);

	/* Resets sel[] in btn_event() */
	gp_widget_bool_set(left, false);
	gp_widget_bool_set(right, false);

	if (match) {
		gp_widget_disable(left);
		gp_widget_disable(right);
		matched++;
		round_matched++;
		status_set(status, GP_WIDGETS_COL_ACCEPT, "Correct: %s = %s",
		           lw->w[from], rw->w[to]);
	} else {
		missed_add(lidx);
		missed_add(ridx);
		mistakes++;
		status_set(status, GP_WIDGETS_COL_ALERT, "Wrong: %s is not %s",
		           lw->w[from], rw->w[to]);
	}

	progress_update();

	if (round_matched == round_size)
		next_round();
}

static int btn_find(gp_widget *btn, unsigned int *col, unsigned int *row)
{
	unsigned int c, r;

	for (c = 0; c < 2; c++) {
		for (r = 0; r < QUIZ_ROWS; r++) {
			if (btns[c][r] == btn) {
				*col = c;
				*row = r;
				return 0;
			}
		}
	}

	return 1;
}

static int btn_event(gp_widget_event *ev)
{
	unsigned int col, row;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (btn_find(ev->self, &col, &row))
		return 0;

	if (!gp_widget_bool_get(ev->self)) {
		if (sel[col] == (int)row)
			sel[col] = -1;
		return 0;
	}

	/* Only one button can be pushed in a column */
	if (sel[col] >= 0)
		gp_widget_bool_set(btns[col][sel[col]], false);

	sel[col] = row;

	if (sel[0] >= 0 && sel[1] >= 0)
		check_pair();

	return 0;
}

void quiz_start(void)
{
	size_t i, len = gp_vec_len(words);
	unsigned int *tmp = realloc(order, len * sizeof(*order));

	if (!tmp)
		return;

	order = tmp;

	tmp = realloc(missed, len * sizeof(*missed));
	if (!tmp)
		return;

	missed = tmp;
	missed_cnt = 0;
	order_len = len;
	order_pos = 0;

	for (i = 0; i < len; i++)
		order[i] = i;

	shuffle(order, len);

	matched = 0;
	mistakes = 0;

	from = dir_from();
	to = !from;

	gp_widget_label_set(lang_label[0], cur_pair->lang[from]);
	gp_widget_label_set(lang_label[1], cur_pair->lang[to]);

	status_set(status, GP_WIDGETS_COL_TEXT, "Match the words with the same meaning");
	progress_update();

	next_round();
}

void quiz_init(gp_htable *uids)
{
	gp_widget *grid = gp_widget_by_uid(uids, "quiz_grid", GP_WIDGET_GRID);
	unsigned int row, col, first_row;

	lang_label[0] = gp_widget_by_uid(uids, "quiz_lang0", GP_WIDGET_LABEL);
	lang_label[1] = gp_widget_by_uid(uids, "quiz_lang1", GP_WIDGET_LABEL);
	status = gp_widget_by_uid(uids, "quiz_status", GP_WIDGET_LABEL);
	progress = gp_widget_by_uid(uids, "quiz_progress", GP_WIDGET_LABEL);

	first_row = gp_widget_grid_rows_append(grid, QUIZ_ROWS);

	for (row = 0; row < QUIZ_ROWS; row++) {
		for (col = 0; col < 2; col++) {
			gp_widget *btn = gp_widget_push_button_new_ev(NULL, GP_WIDGET_STOCK_NONE,
			                                              GP_WIDGET_STOCK_NONE,
			                                              false, btn_event, NULL);
			btn->align = GP_FILL;
			gp_widget_grid_put(grid, col, first_row + row, btn);
			btns[col][row] = btn;
		}
	}
}
