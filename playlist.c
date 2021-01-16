//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2021 Cyril Hrubis <metan@ucw.cz>

 */

#define _GNU_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <utils/gp_vec.h>

#include "playlist.h"
#include "widgets/gp_widgets.h"

struct playlist self;

void playlist_init(void)
{
	self.cur = 0;
	self.loop = 0;
	self.shuffle = 0;
	self.files = gp_vec_new(0, sizeof(char *));
}

int playlist_next(void)
{
	if (self.shuffle) {
		//TODO
	}

	if (self.cur + 1 >= gp_vec_len(self.files)) {
		if (self.loop) {
			self.cur = 0;
			return 1;
		}
	} else {
		self.cur++;
		return 1;
	}

	return 0;
}

int playlist_prev(void)
{
	if (self.shuffle) {
		//TODO
	}

	if (self.cur == 0) {
		if (self.loop) {
			self.cur = gp_vec_len(self.files) - 1;
			return 1;
		}
	} else {
		self.cur--;
		return 1;
	}

	return 0;
}

int playlist_move_up(size_t pos)
{
	if (pos <= 0)
		return 0;

	if (pos >= gp_vec_len(self.files))
		return 0;

	char *tmp = self.files[pos-1];
	self.files[pos-1] = self.files[pos];
	self.files[pos] = tmp;

	return 1;
}

int playlist_move_down(size_t pos)
{
	if (pos + 1 >= gp_vec_len(self.files))
		return 0;

	char *tmp = self.files[pos+1];
	self.files[pos+1] = self.files[pos];
	self.files[pos] = tmp;

	return 1;
}

int playlist_set(size_t pos)
{
	if (pos >= gp_vec_len(self.files))
		return 0;

	self.cur = pos;
	return 1;
}

const char *playlist_cur(void)
{
	if (!gp_vec_len(self.files))
		return NULL;

	return self.files[self.cur];
}

static void add_path(const char *path, const char *fname)
{
	void *new;
	size_t len = gp_vec_len(self.files);

	new = gp_vec_append(self.files, 1);
	if (!new)
		return;

	self.files = new;

	if (path) {
		if (asprintf(&self.files[len], "%s/%s", path, fname) < 0)
			self.files[len] = NULL;
	} else {
		self.files[len] = strdup(fname);
	}

	if (!self.files[len])
		self.files = gp_vec_remove(self.files, 1);
}

void playlist_add(const char *path)
{
	struct stat path_stat;
	DIR *dir;
	struct dirent *ent;

	stat(path, &path_stat);

	if (S_ISREG(path_stat.st_mode)) {
		add_path(NULL, path);
		return;
	}

	dir = opendir(path);
	if (!dir)
		return;

	while ((ent = readdir(dir))) {
		if (!(ent->d_type & DT_REG))
			continue;

		size_t name_len = strlen(ent->d_name);

		if (name_len < 4)
			continue;

		if (!strcmp(ent->d_name + name_len - 3, ".mp3"))
			continue;

		add_path(path, ent->d_name);
	}

	closedir(dir);
}

void playlist_rem(size_t off, size_t len)
{
	size_t i;

	if (off >= gp_vec_len(self.files))
		return;

	len = GP_MIN(gp_vec_len(self.files) - off, len);

	for (i = 0; i < len; i++)
		free(self.files[i + off]);

	self.files = gp_vec_delete(self.files, off, len);
}

void playlist_list(void)
{
	size_t i;

	printf("PLAYLIST\n--------\n\n");

	for (i = 0; i < gp_vec_len(self.files); i++)
		printf("- '%s'\n", self.files[i]);
}

static unsigned int spos = 0;

int playlist_set_row(struct gp_widget *table, int op, unsigned int pos)
{
	(void) table;

	switch (op) {
	case GP_TABLE_ROW_RESET:
		spos = 0;
	break;
	case GP_TABLE_ROW_ADVANCE:
		if (spos + pos >= gp_vec_len(self.files))
			return 0;

		spos += pos;
		return 1;
	break;
	case GP_TABLE_ROW_TELL:
		return gp_vec_len(self.files);
	break;
	}

	return 0;
}

const char *playlist_get(struct gp_widget *table, unsigned int col)
{
	(void) table;

	if (col)
		return NULL;

	const char *fname = rindex(self.files[spos], '/');
	if (fname)
		fname++;
	else
		fname = self.files[spos];

	return fname;
}
