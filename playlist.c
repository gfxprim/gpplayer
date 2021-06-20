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

const char *save_path;

void playlist_init(const char *path)
{
	self.cur = 0;
	self.loop = 0;
	self.shuffle = 0;
	self.files = gp_vec_new(0, sizeof(char *));

	if (path) {
		save_path = strdup(path);
		playlist_load(path);
	}
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int creat_cfg_file(const char *path, mode_t dir_mode, mode_t file_mode)
{
	char *full_path, *del;
	int fd;

	if (!access(path, W_OK))
		return open(path, O_WRONLY);

	char *home_path = getenv("HOME");

	if (!home_path) {
		errno = ENOENT;
		return -1;
	}

	asprintf(&full_path, "%s/.config/%s", home_path, strdup(path));
	if (!full_path) {
		errno = ENOMEM;
		return -1;
	}

	del = full_path;

	for (;;) {
		del = strchr(del+1, '/');
		if (!del)
			break;

		*del = 0;
		if (mkdir(full_path, dir_mode)) {
			if (errno != EEXIST)
				return -1;
		}
		*del = '/';
	}

	fd = creat(full_path, file_mode);
	free(full_path);
	return fd;
}

int open_cfg_file(const char *path)
{
	char *home_path, *full_path = NULL;
	int fd;

	home_path = getenv("HOME");
	if (!home_path) {
		errno = ENOENT;
		return -1;
	}

	asprintf(&full_path, "%s/.config/%s", home_path, path);
	if (!full_path) {
		errno = ENOMEM;
		return -1;
	}

	fd = open(full_path, O_RDONLY);
	free(full_path);
	return fd;
}

void playlist_exit(void)
{
	if (!save_path)
		return;

	playlist_save(save_path);
}

static void add_path(const char *path, const char *fname)
{
	void *new;
	size_t len = gp_vec_len(self.files);
	const char *cwd = getenv("PWD");

	new = gp_vec_expand(self.files, 1);
	if (!new)
		return;

	self.files = new;

	if (path) {
		if (path[0] == '/') {
			if (asprintf(&self.files[len], "%s/%s", path, fname) < 0)
				self.files[len] = NULL;
		} else {
			if (asprintf(&self.files[len], "%s/%s/%s", cwd, path, fname) < 0)
				self.files[len] = NULL;
		}
	} else {
		if (fname[0] == '/') {
			self.files[len] = strdup(fname);
		} else {
			if (asprintf(&self.files[len], "%s/%s", cwd, fname) < 0)
				self.files[len] = NULL;
		}
	}

	if (!self.files[len])
		self.files = gp_vec_shrink(self.files, 1);
}

void playlist_load(const char *path)
{
	int fd = open_cfg_file(path);
	char buf[4096];

	if (fd < 0)
		return;

	FILE *f = fdopen(fd, "r");

	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		if (buf[len-1] == '\n')
			buf[len-1] = 0;
		if (!buf[0])
			continue;
		add_path(NULL, buf);
	}

	fclose(f);
}

void playlist_save(const char *path)
{
	int fd;
	size_t i;

	fd = creat_cfg_file(path, 0755, 0644);
	if (fd < 0)
		return;

	for (i = 0; i < gp_vec_len(self.files); i++) {
		write(fd, self.files[i], strlen(self.files[i]));
		write(fd, "\n", 1);
	}

	close(fd);
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

static int cmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
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

	size_t first = gp_vec_len(self.files);

	while ((ent = readdir(dir))) {
		if (!(ent->d_type & DT_REG))
			continue;

		size_t name_len = strlen(ent->d_name);

		if (name_len < 4)
			continue;

		if (strcmp(ent->d_name + name_len - 4, ".mp3"))
			continue;

		add_path(path, ent->d_name);
	}

	size_t last = gp_vec_len(self.files);

	qsort(&self.files[first], last-first, sizeof(char*), cmp);

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

	self.files = gp_vec_del(self.files, off, len);
}

void playlist_clear(void)
{
	size_t i;

	for (i = 0; i < gp_vec_len(self.files); i++)
		free(self.files[i]);

	self.files = gp_vec_resize(self.files, 0);
}

void playlist_list(void)
{
	size_t i;

	printf("PLAYLIST\n--------\n\n");

	for (i = 0; i < gp_vec_len(self.files); i++)
		printf("- '%s'\n", self.files[i]);
}

static unsigned int spos = 0;

int playlist_set_row(gp_widget *table, int op, unsigned int pos)
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

gp_widget_table_cell *playlist_get(gp_widget *table, unsigned int col)
{
	static gp_widget_table_cell cell;
	(void) table;

	if (col)
		return NULL;

	const char *fname = rindex(self.files[spos], '/');
	if (fname)
		fname++;
	else
		fname = self.files[spos];

	cell.text = fname;
	cell.bold = (spos == self.cur);

	return &cell;
}
