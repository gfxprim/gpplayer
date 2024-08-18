//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

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
#include "gpplayer_conf.h"

struct playlist_file {
	/** Randomized index for a shuffle. */
	size_t shuffle_idx;
	/** Path to the music file. */
	char *file;
};

struct playlist {
	size_t cur;
	struct playlist_file *files;
};

struct playlist playlist;

const char *save_path;

void playlist_init(const char *path)
{
	playlist.cur = 0;
	playlist.files = gp_vec_new(0, sizeof(struct playlist_file));

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
			if (asprintf(&playlist.files[len].file, "%s/%s", path, fname) < 0)
				playlist.files[len].file = NULL;
		} else {
			if (asprintf(&playlist.files[len].file, "%s/%s/%s", cwd, path, fname) < 0)
				playlist.files[len].file = NULL;
		}
	} else {
		if (fname[0] == '/') {
			playlist.files[len].file = strdup(fname);
		} else {
			if (asprintf(&playlist.files[len].file, "%s/%s", cwd, fname) < 0)
				playlist.files[len].file = NULL;
		}
	}

	if (!playlist.files[len].file) {
		playlist.files = gp_vec_shrink(playlist.files, 1);
		return;
	}

	size_t new_idx = gp_vec_len(playlist.files) - 1;
	size_t rnd_idx = new_idx ? random() % new_idx : 0;

	playlist.files[new_idx].shuffle_idx = playlist.files[rnd_idx].shuffle_idx;
	playlist.files[rnd_idx].shuffle_idx = new_idx;
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
		write(fd, playlist.files[i].file, strlen(playlist.files[i].file));
		write(fd, "\n", 1);
	}

	close(fd);
}

int playlist_next(void)
{
	if (playlist.cur + 1 >= gp_vec_len(playlist.files)) {
		if (gpplayer_conf->playlist_repeat) {
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
	if (playlist.cur == 0) {
		if (gpplayer_conf->playlist_repeat) {
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

	GP_SWAP(playlist.files[pos-1].file, playlist.files[pos].file);

	return 1;
}

int playlist_move_down(size_t pos)
{
	if (pos + 1 >= gp_vec_len(playlist.files))
		return 0;

	GP_SWAP(playlist.files[pos+1].file, playlist.files[pos].file);

	return 1;
}

int playlist_set(size_t pos)
{
	size_t i;

	if (pos >= gp_vec_len(playlist.files))
		return 0;

	if (gpplayer_conf->playlist_shuffle) {
		for (i = 0; i < gp_vec_len(playlist.files); i++) {
			if (playlist.files[i].shuffle_idx == pos) {
				pos = i;
				break;
			}
		}
	}

	playlist.cur = pos;
	return 1;
}

static size_t playlist_cur_idx(void)
{
	size_t cur_idx = playlist.cur;

	if (gpplayer_conf->playlist_shuffle)
		cur_idx = playlist.files[cur_idx].shuffle_idx;

	return cur_idx;
}

const char *playlist_cur(void)
{
	if (!gp_vec_len(playlist.files))
		return NULL;

	return playlist.files[playlist_cur_idx()].file;
}

static int cmp(const void *a, const void *b)
{
	const struct playlist_file *fa = a;
	const struct playlist_file *fb = b;

	return strcmp(fa->file, fb->file);
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

		//if (strcmp(ent->d_name + name_len - 4, ".mp3"))
		//	continue;

		add_path(path, ent->d_name);
	}

	size_t last = gp_vec_len(playlist.files);

	qsort(&playlist.files[first], last-first, sizeof(struct playlist_file), cmp);

	closedir(dir);
}

static size_t shuffle_idx_max(void)
{
	size_t i, max = 0, ret = 0;

	for (i = 0; i < gp_vec_len(playlist.files); i++) {
		if (playlist.files[i].shuffle_idx > max) {
			max = playlist.files[i].shuffle_idx;
			ret = i;
		}
	}

	return ret;
}

void playlist_rem(size_t off, size_t len)
{
	size_t i;

	if (off >= gp_vec_len(playlist.files))
		return;

	len = GP_MIN(gp_vec_len(playlist.files) - off, len);

	for (i = 0; i < len; i++) {
		size_t rem_idx = i+off;
		size_t max_idx = shuffle_idx_max();

		playlist.files[max_idx].shuffle_idx = playlist.files[rem_idx].shuffle_idx;

		free(playlist.files[rem_idx].file);
	}

	playlist.files = gp_vec_del(playlist.files, off, len);
}

void playlist_clear(void)
{
	size_t i;

	for (i = 0; i < gp_vec_len(playlist.files); i++)
		free(playlist.files[i].file);

	playlist.files = gp_vec_resize(playlist.files, 0);
}

void playlist_list(void)
{
	size_t i;

	printf("PLAYLIST\n--------\n\n");

	for (i = 0; i < gp_vec_len(playlist.files); i++) {
		printf("- (c=%3zu s=%3zu) '%s'\n", i,
		       playlist.files[i].shuffle_idx,
		       playlist.files[i].file);
	}
}

void playlist_shuffle_set(bool shuffle)
{
	gpplayer_conf_playlist_shuffle_set(shuffle);

	if (!shuffle && playlist.files)
		playlist.cur = playlist.files[playlist.cur].shuffle_idx;
}

void playlist_repeat_set(bool repeat)
{
	gpplayer_conf_playlist_repeat_set(repeat);
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

	const char *fname = rindex(playlist.files[row].file, '/');
	if (fname)
		fname++;
	else
		fname = playlist.files[row].file;

	size_t cur_idx = playlist_cur_idx();

	cell->text = fname;
	cell->tattr = (row == cur_idx) ? GP_TATTR_BOLD : 0;

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
