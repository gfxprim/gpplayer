//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2021 Cyril Hrubis <metan@ucw.cz>

 */

#define _GNU_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <utils/gp_vec.h>
#include <widgets/gp_widgets.h>

#include "playlist.h"

struct playlist playlist;

const char *save_path;

void playlist_init(const char *path)
{
	playlist.cur = 0;
	playlist.loop = 0;
	playlist.shuffle = 0;
	playlist.files = gp_vec_new(0, sizeof(char *));

	if (path) {
		save_path = strdup(path);
		playlist_load(path);
	}
}

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
	size_t len = gp_vec_len(playlist.files);
	const char *cwd = getenv("PWD");

	new = gp_vec_expand(playlist.files, 1);
	if (!new)
		return;

	playlist.files = new;

	if (path) {
		if (path[0] == '/') {
			if (asprintf(&playlist.files[len], "%s/%s", path, fname) < 0)
				playlist.files[len] = NULL;
		} else {
			if (asprintf(&playlist.files[len], "%s/%s/%s", cwd, path, fname) < 0)
				playlist.files[len] = NULL;
		}
	} else {
		if (fname[0] == '/') {
			playlist.files[len] = strdup(fname);
		} else {
			if (asprintf(&playlist.files[len], "%s/%s", cwd, fname) < 0)
				playlist.files[len] = NULL;
		}
	}

	if (!playlist.files[len])
		playlist.files = gp_vec_shrink(playlist.files, 1);
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

	for (i = 0; i < gp_vec_len(playlist.files); i++) {
		write(fd, playlist.files[i], strlen(playlist.files[i]));
		write(fd, "\n", 1);
	}

	close(fd);
}

int playlist_next(void)
{
	if (playlist.shuffle) {
		//TODO
	}

	if (playlist.cur + 1 >= gp_vec_len(playlist.files)) {
		if (playlist.loop) {
			playlist.cur = 0;
			return 1;
		}
	} else {
		playlist.cur++;
		return 1;
	}

	return 0;
}

int playlist_prev(void)
{
	if (playlist.shuffle) {
		//TODO
	}

	if (playlist.cur == 0) {
		if (playlist.loop) {
			playlist.cur = gp_vec_len(playlist.files) - 1;
			return 1;
		}
	} else {
		playlist.cur--;
		return 1;
	}

	return 0;
}

int playlist_move_up(size_t pos)
{
	if (pos <= 0)
		return 0;

	if (pos >= gp_vec_len(playlist.files))
		return 0;

	char *tmp = playlist.files[pos-1];
	playlist.files[pos-1] = playlist.files[pos];
	playlist.files[pos] = tmp;

	return 1;
}

int playlist_move_down(size_t pos)
{
	if (pos + 1 >= gp_vec_len(playlist.files))
		return 0;

	char *tmp = playlist.files[pos+1];
	playlist.files[pos+1] = playlist.files[pos];
	playlist.files[pos] = tmp;

	return 1;
}

int playlist_set(size_t pos)
{
	if (pos >= gp_vec_len(playlist.files))
		return 0;

	playlist.cur = pos;
	return 1;
}

const char *playlist_cur(void)
{
	if (!gp_vec_len(playlist.files))
		return NULL;

	return playlist.files[playlist.cur];
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

	size_t first = gp_vec_len(playlist.files);

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

	size_t last = gp_vec_len(playlist.files);

	qsort(&playlist.files[first], last-first, sizeof(char*), cmp);

	closedir(dir);
}

void playlist_rem(size_t off, size_t len)
{
	size_t i;

	if (off >= gp_vec_len(playlist.files))
		return;

	len = GP_MIN(gp_vec_len(playlist.files) - off, len);

	for (i = 0; i < len; i++)
		free(playlist.files[i + off]);

	playlist.files = gp_vec_del(playlist.files, off, len);
}

void playlist_clear(void)
{
	size_t i;

	for (i = 0; i < gp_vec_len(playlist.files); i++)
		free(playlist.files[i]);

	playlist.files = gp_vec_resize(playlist.files, 0);
}

void playlist_list(void)
{
	size_t i;

	printf("PLAYLIST\n--------\n\n");

	for (i = 0; i < gp_vec_len(playlist.files); i++)
		printf("- '%s'\n", playlist.files[i]);
}

static int playlist_seek_row(gp_widget *self, int op, unsigned int pos)
{
	gp_widget_table *table = self->tbl;

	switch (op) {
	case GP_TABLE_ROW_RESET:
		table->row_idx = 0;
	break;
	case GP_TABLE_ROW_ADVANCE:
		if (table->row_idx + pos >= gp_vec_len(playlist.files))
			return 0;

		table->row_idx += pos;
		return 1;
	break;
	case GP_TABLE_ROW_MAX:
		return gp_vec_len(playlist.files);
	break;
	}

	return 0;
}

static int playlist_get(gp_widget *self, gp_widget_table_cell *cell, unsigned int col_id)
{
	unsigned int row = self->tbl->row_idx;

	if (col_id)
		return 0;

	const char *fname = rindex(playlist.files[row], '/');
	if (fname)
		fname++;
	else
		fname = playlist.files[row];

	cell->text = fname;
	cell->tattr = (row == playlist.cur) ? GP_TATTR_BOLD : 0;

	return 1;
}

const gp_widget_table_col_ops playlist_ops = {
	.seek_row = playlist_seek_row,
	.get_cell = playlist_get,
	.col_map = {
		{.id = "name", .idx = 0},
		{}
	}
};
