//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <string.h>

#include <utils/gp_json_serdes.h>
#include <utils/gp_app_cfg.h>
#include <utils/gp_path.h>
#include <widgets/gp_app_info.h>

#include "audio_decoder.h"
#include "gpplayer_conf.h"

static struct gpplayer_conf conf = {
	.softvol = 100,
};

const struct gpplayer_conf *gpplayer_conf = &conf;

static gp_json_struct conf_desc[] = {
	GP_JSON_SERDES_STR_CPY(struct gpplayer_conf, decoder, 0, sizeof(conf.decoder)),
	GP_JSON_SERDES_STR_DUP(struct gpplayer_conf, last_dialog_path, 0, SIZE_MAX),
	GP_JSON_SERDES_BOOL(struct gpplayer_conf, playlist_repeat, 0),
	GP_JSON_SERDES_BOOL(struct gpplayer_conf, playlist_shuffle, 0),
	GP_JSON_SERDES_UINT8(struct gpplayer_conf, softvol, 0, 0, AUDIO_DECODER_SOFTVOL_MAX),
	{}
};

void gpplayer_conf_load(void)
{
        char *conf_path;

	conf_path = gp_app_cfg_path(gp_app_info_name(), "config.json");
        if (!conf_path)
                return;

	gp_json_load_struct(conf_path, conf_desc, &conf);

	free(conf_path);
}

void gpplayer_conf_save(void)
{
	char *conf_path;

	if (!conf.dirty)
		return;

	if (gp_app_cfg_mkpath(gp_app_info_name()))
		return;

	conf_path = gp_app_cfg_path(gp_app_info_name(), "config.json");
        if (!conf_path)
                return;

	gp_json_save_struct(conf_path, conf_desc, &conf);

	free(conf_path);

	conf.dirty = 0;
}

void gpplayer_conf_decoder_set(const char *decoder)
{
	conf.dirty = 1;

	strncpy(conf.decoder, decoder, sizeof(conf.decoder)-1);
}

void gpplayer_conf_softvol_set(uint8_t softvol)
{
	if (softvol > AUDIO_DECODER_SOFTVOL_MAX)
		softvol = AUDIO_DECODER_SOFTVOL_MAX;

	if (softvol == conf.softvol)
		return;

	conf.softvol = softvol;
	conf.dirty = 1;
}

void gpplayer_conf_last_dialog_path_set(const char *path)
{
	char *new_path = gp_dirname(path);

	if (!new_path)
		return;

	free(conf.last_dialog_path);

	conf.last_dialog_path = new_path;

	conf.dirty = 1;
}

void gpplayer_conf_playlist_repeat_set(bool val)
{
	if (conf.playlist_repeat == val)
		return;

	conf.playlist_repeat = val;
	conf.dirty = 1;
}

void gpplayer_conf_playlist_shuffle_set(bool val)
{
	if (conf.playlist_shuffle == val)
		return;

	conf.playlist_shuffle = val;
	conf.dirty = 1;
}
