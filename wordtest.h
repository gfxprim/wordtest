//SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

/**
 * @file wordtest.h
 * @brief Shared wordtest data structures and functions.
 */

#ifndef WORDTEST_H
#define WORDTEST_H

#include <gfxprim.h>

/**
 * @brief A word pair.
 *
 * Each string may contain several alternatives separated by ','.
 */
struct word {
	/**
	 * @brief Words in the first and second language of the language pair.
	 */
	char *w[2];
};

/**
 * @brief A lesson, i.e. a single lesson file.
 */
struct lesson {
	/** @brief A lesson name shown in the menu. */
	char *name;
	/** @brief A file name, used to remember the selection. */
	char *file;
	/** @brief A path to the lesson file. */
	char *path;
	/** @brief A number of words in the lesson. */
	size_t word_cnt;
};

/**
 * @brief A course, i.e. a directory with lessons.
 */
struct course {
	/** @brief A directory name, used to remember the selection. */
	char *id;
	/** @brief A course name shown in the menu. */
	char *name;
	/** @brief A gp_vec of lessons sorted by file name. */
	struct lesson *lessons;
	/** @brief A number of words in all lessons. */
	size_t word_cnt;
};

/**
 * @brief A language pair, i.e. a directory with courses.
 */
struct lang_pair {
	/** @brief A directory name e.g. "en-cz". */
	char *id;
	/** @brief Language names e.g. "English" and "Czech". */
	const char *lang[2];
	/** @brief A name shown in the menu e.g. "English - Czech". */
	char *label;
	/** @brief A gp_vec of courses. */
	struct course *courses;
};

/**
 * @brief Looks up courses in the search paths.
 *
 * Pairs and courses found in more than one search path are taken from the
 * first one.
 *
 * @return A gp_vec of language pairs, may be NULL if nothing was found.
 */
struct lang_pair *courses_discover(void);

/**
 * @brief Creates a language pair with a single course from a lesson file.
 *
 * The language pair and the course are derived from the directory names if
 * the file is stored in the pair/course/lesson.txt structure.
 *
 * @param path A path to a lesson file.
 *
 * @return A gp_vec with a single language pair or NULL on a failure.
 */
struct lang_pair *courses_from_file(const char *path);

/**
 * @brief Returns course search paths.
 *
 * @return A NULL terminated array of paths.
 */
const char *const *courses_search_paths(void);

/**
 * @brief Parses a lesson file.
 *
 * @param path A path to a lesson file.
 * @param title If not NULL and the file has a '# title:' line a copy of the
 *              title is stored there, unless it's already set.
 * @param words If not NULL the words are appended to the gp_vec.
 *
 * @return A number of words or -1 on a failure.
 */
ssize_t lesson_load(const char *path, char **title, struct word **words);

/**
 * @brief Picks random words from a gp_vec of words, frees the rest.
 *
 * @param words A gp_vec of words.
 * @param cnt A maximal number of words to keep.
 *
 * @return A (possibly new) pointer to the gp_vec of words.
 */
struct word *words_pick(struct word *words, size_t cnt);

/**
 * @brief Frees a gp_vec of words.
 *
 * @param words A gp_vec of words, may be NULL.
 */
void words_free(struct word *words);

/**
 * @brief Checks an answer.
 *
 * Whitespaces are trimmed and case is ignored, everything else, including
 * diacritics, has to match exactly.
 *
 * @param expected Comma separated list of correct answers.
 * @param answer An answer to check.
 *
 * @return Non-zero if the answer matches any of the alternatives.
 */
int answer_matches(const char *expected, const char *answer);

/**
 * @brief Shuffles an array.
 *
 * @param arr An array to shuffle.
 * @param len An array length.
 */
void shuffle(unsigned int *arr, size_t len);

/**
 * @brief A test direction.
 */
enum direction {
	/** @brief From lang[1] to lang[0], e.g. Czech -> English for en-cz. */
	DIR_BACKWARD,
	/** @brief From lang[0] to lang[1]. */
	DIR_FORWARD,
	/** @brief Randomly chosen for each word. */
	DIR_MIXED,
};

/** @brief Currently selected language pair. */
extern const struct lang_pair *cur_pair;

/** @brief A gp_vec of words for the current test. */
extern struct word *words;

/**
 * @brief Returns a language words are shown in.
 *
 * For DIR_MIXED the language is chosen randomly on each call.
 *
 * @return An index into the struct word w array.
 */
unsigned int dir_from(void);

/**
 * @brief Switches back to the main menu.
 */
void show_menu(void);

/**
 * @brief Switches to the results screen.
 *
 * @param title A results title e.g. "Quiz finished".
 * @param summary A one line summary e.g. the score.
 * @param missed Indexes into words that were missed, the array has to stay
 *               valid until the next test is started.
 * @param missed_cnt A number of missed words.
 */
void show_results(const char *title, const char *summary,
                  const unsigned int *missed, size_t missed_cnt);

/**
 * @brief Sets a status label text and color.
 *
 * @param label A label widget.
 * @param color A text color.
 * @param fmt A printf-like format string.
 * @param ... A printf-like parameters.
 */
void status_set(gp_widget *label, enum gp_widgets_color color, const char *fmt, ...)
                __attribute__((format (printf, 3, 4)));

/**
 * @brief Initializes the quiz, looks up widgets.
 *
 * @param uids Widget uids from the layout.
 */
void quiz_init(gp_htable *uids);

/**
 * @brief Starts a quiz with the current words.
 */
void quiz_start(void);

/**
 * @brief Initializes the written exam, looks up widgets.
 *
 * @param uids Widget uids from the layout.
 */
void exam_init(gp_htable *uids);

/**
 * @brief Starts a written exam with the current words.
 */
void exam_start(void);

#endif /* WORDTEST_H */
