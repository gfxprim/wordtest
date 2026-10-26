//SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wordtest.h"

static gp_widget *prompt;
static gp_widget *word_label;
static gp_widget *answer;
static gp_widget *feedback;
static gp_widget *progress;

struct item {
	unsigned int word;
	/* Language the word is shown in */
	unsigned int from;
};

/* Words asked in the current pass */
static struct item *queue;
static size_t queue_len;
static size_t queue_pos;

/* Words missed in the current pass, asked again in the next one */
static struct item *missed;
static size_t missed_len;

/* Words missed in the first pass, listed at the end */
static unsigned int *first_missed;
static size_t first_missed_len;

static int first_pass;
static unsigned int correct;
static size_t total;

static void items_shuffle(struct item *items, size_t len)
{
	size_t i;

	for (i = len; i > 1; i--) {
		size_t j = random() % i;
		struct item tmp = items[i-1];

		items[i-1] = items[j];
		items[j] = tmp;
	}
}

static void progress_update(void)
{
	if (first_pass) {
		gp_widget_label_printf(progress, "%zu / %zu, %u correct",
		                       queue_pos + 1, total, correct);
	} else {
		gp_widget_label_printf(progress, "Repeating missed words: %zu left",
		                       queue_len - queue_pos);
	}
}

static void show_question(void)
{
	const struct item *item = &queue[queue_pos];

	gp_widget_label_printf(prompt, "Translate to %s", cur_pair->lang[!item->from]);
	gp_widget_label_set(word_label, words[item->word].w[item->from]);
	gp_widget_tbox_clear(answer);

	progress_update();
}

static void finish(void)
{
	char summary[128];

	snprintf(summary, sizeof(summary), "Score: %u / %zu (%zu %%)",
	         correct, total, (100 * correct) / total);

	show_results("Exam finished", summary, first_missed, first_missed_len);
}

static void next_pass(void)
{
	struct item *tmp = queue;

	queue = missed;
	queue_len = missed_len;
	queue_pos = 0;

	missed = tmp;
	missed_len = 0;

	first_pass = 0;

	items_shuffle(queue, queue_len);
}

static void check_answer(void)
{
	const char *text = gp_widget_tbox_text(answer);
	const struct item *item = &queue[queue_pos];
	const struct word *word = &words[item->word];
	const char *asked = word->w[item->from];
	const char *expected = word->w[!item->from];

	if (gp_widget_tbox_is_empty(answer))
		return;

	if (answer_matches(expected, text)) {
		correct += first_pass;
		status_set(feedback, GP_WIDGETS_COL_ACCEPT, "Correct: %s = %s",
		           asked, expected);
	} else {
		status_set(feedback, GP_WIDGETS_COL_ALERT, "Wrong: %s = %s (you wrote: %s)",
		           asked, expected, text);

		missed[missed_len++] = *item;

		if (first_pass)
			first_missed[first_missed_len++] = item->word;
	}

	if (++queue_pos >= queue_len) {
		if (!missed_len) {
			finish();
			return;
		}

		next_pass();
	}

	show_question();
}

int exam_answer_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (ev->sub_type == GP_WIDGET_TBOX_TRIGGER)
		check_answer();

	return 0;
}

int exam_check(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	check_answer();
	gp_widget_focus_set(answer);

	return 0;
}

static void *array_resize(void *arr, size_t len, size_t unit)
{
	void *tmp = realloc(arr, len * unit);

	if (!tmp)
		free(arr);

	return tmp;
}

void exam_start(void)
{
	size_t i;

	total = gp_vec_len(words);

	queue = array_resize(queue, total, sizeof(*queue));
	missed = array_resize(missed, total, sizeof(*missed));
	first_missed = array_resize(first_missed, total, sizeof(*first_missed));

	if (!queue || !missed || !first_missed) {
		GP_WARN("Malloc failed :-(");
		show_menu();
		return;
	}

	for (i = 0; i < total; i++) {
		queue[i].word = i;
		queue[i].from = dir_from();
	}

	items_shuffle(queue, total);

	queue_len = total;
	queue_pos = 0;
	missed_len = 0;
	first_missed_len = 0;
	first_pass = 1;
	correct = 0;

	status_set(feedback, GP_WIDGETS_COL_TEXT, "Type the translation and press Enter");
	show_question();

	gp_widget_focus_set(answer);
}

void exam_init(gp_htable *uids)
{
	prompt = gp_widget_by_uid(uids, "exam_prompt", GP_WIDGET_LABEL);
	word_label = gp_widget_by_uid(uids, "exam_word", GP_WIDGET_LABEL);
	answer = gp_widget_by_uid(uids, "exam_answer", GP_WIDGET_TBOX);
	feedback = gp_widget_by_uid(uids, "exam_feedback", GP_WIDGET_LABEL);
	progress = gp_widget_by_uid(uids, "exam_progress", GP_WIDGET_LABEL);
}
