//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef GPPLAYER_CONF_H
#define GPPLAYER_CONF_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief A gpplayer configuration.
 */
struct gpplayer_conf {
	/** @brief Selected decoder. */
	char decoder[32];
	/** @brief Saved softvolume. */
	uint8_t softvol;
	/** @brief Last dialog file open path. */
	char *last_dialog_path;

	/** @brief Shuffle playlist while playing. */
	bool playlist_shuffle;
	/** @brief Repeat the playlist */
	bool playlist_repeat;

	/** @brief Set if any data was change and needs to be saved. */
	uint8_t dirty:1;
};

extern const struct gpplayer_conf *gpplayer_conf;

/**
 * @brief Loads a gpplayer configuration from a .config/ directory.
 */
void gpplayer_conf_load(void);

/**
 * @brief Saves a gpplayer configuration to a .config/ directory.
 */
void gpplayer_conf_save(void);

/**
 * @brief Sets the config decoder.
 *
 * @param decoder A decoder name.
 */
void gpplayer_conf_decoder_set(const char *decoder);

/**
 * @brief Sets a softvolume.
 *
 * @param softvol A softvolume.
 */
void gpplayer_conf_softvol_set(uint8_t softvol);

/**
 * @brief Sets a last dialog path
 *
 * If last component of the path is a filename it's stripped automatically.
 *
 * @param A path.
 */
void gpplayer_conf_last_dialog_path_set(const char *path);

/**
 * @brief Sets playlist repeat value.
 *
 * @val A playlist repeat value.
 */
void gpplayer_conf_playlist_repeat_set(bool val);

/**
 * @brief Sets playlist shuffle value.
 *
 * @val A playlist shuffle value.
 */
void gpplayer_conf_playlist_shuffle_set(bool val);

#endif /* GPPLAYER_CONF_H */
