//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2021 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef PLAYLIST_H__
#define PLAYLIST_H__

struct playlist {
	unsigned int cur;
	unsigned int loop:1;
	unsigned int shuffle:1;
	char **files;
};

/*
 * @brief Inialize playlist strucutres.
 *
 * @path If non-NULL playlist is loaded from the file.
 */
void playlist_init(const char *path);

void playlist_exit(void);

int playlist_next(void);

int playlist_prev(void);

int playlist_move_up(size_t pos);

int playlist_move_down(size_t pos);

int playlist_set(size_t pos);

const char *playlist_cur(void);

void playlist_add(const char *path);

void playlist_rem(size_t off, size_t len);

void playlist_list(void);

void playlist_save(const char *fname);

void playlist_load(const char *fname);

#endif /* PLAYLIST_H__ */
