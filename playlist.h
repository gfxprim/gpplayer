//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef PLAYLIST_H__
#define PLAYLIST_H__

/**
 * @brief Inialize playlist strucutres.
 *
 * @path If non-NULL playlist is loaded from the file.
 */
void playlist_init(const char *path);

/**
 * @brief Frees the playlist memory.
 */
void playlist_exit(void);

/**
 * @brief Move to the next song in playlist.
 *
 * @return Zero if there is no more songs in the list, non-zero otherwise.
 */
int playlist_next(void);

/**
 * @brief Move to the previous song in playlist.
 *
 * @return Zero if there is no more songs in the list, non-zero otherwise.
 */
int playlist_prev(void);

/**
 * @brief Sets current playlist position.
 *
 * @param pos A position in the playlist.
 * @return Zero if the postion is out of playlist, non-zero otherwise.
 */
int playlist_set(size_t pos);

/**
 * @brief Returns a path to the current playlist song.
 *
 * @return A path to the current playlist song.
 */
const char *playlist_cur(void);

/**
 * @brief Moves a song one position up.
 *
 * @param pos A position in the playlist.
 * @return Zero if song couldn't be moved, non-zero otherwise.
 */
int playlist_move_up(size_t pos);

/**
 * @brief Moves a song one position down.
 *
 * @param pos A position in the playlist.
 * @return Zero if song couldn't be moved, non-zero otherwise.
 */
int playlist_move_down(size_t pos);

/**
 * @brief Add song(s) to the playlist.
 *
 * If path points to a directory it's scanned for music files.
 *
 * @param path A path to a file or a directory.
 */
void playlist_add(const char *path);

/**
 * @brief Removes songs from the playlist.
 *
 * @param off Offset to first song to remove.
 * @param len Number of songs to remove.
 */
void playlist_rem(size_t off, size_t len);

/**
 * @brief Removes all songs from the playlist.
 */
void playlist_clear(void);

void playlist_list(void);

void playlist_save(const char *fname);

void playlist_load(const char *fname);

/**
 * @brief Sets shuffle state.
 *
 * @param shuffle A shuffle value.
 */
void playlist_shuffle_set(bool shuffle);

/**
 * @brief Sets repeat state.
 *
 * @param repeat A repeat value.
 */
void playlist_repeat_set(bool repeat);

#endif /* PLAYLIST_H__ */
