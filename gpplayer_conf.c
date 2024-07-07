//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <string.h>

#include <utils/gp_json_serdes.h>
#include <utils/gp_app_cfg.h>
#include <widgets/gp_app_info.h>

#include "audio_decoder.h"
#include "gpplayer_conf.h"

static struct gpplayer_conf conf = {
	.softvol = 100,
};

const struct gpplayer_conf *gpplayer_conf = &conf;

static gp_json_struct conf_desc[] = {
	GP_JSON_SERDES_STR_CPY(struct gpplayer_conf, decoder, 0, sizeof(conf.decoder), "decoder"),
	GP_JSON_SERDES_UINT8(struct gpplayer_conf, softvol, 0, 0, AUDIO_DECODER_SOFTVOL_MAX, "softvol"),
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