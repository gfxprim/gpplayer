//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <string.h>
#include <mpv/client.h>
#include <core/gp_debug.h>

static const struct audio_decoder_callbacks *audio_decoder_cbs;

#include "audio_decoder_priv.h"

static mpv_handle *ctx;

static int audio_decoder_track_load_mpv(const char *path)
{
	const char *cmd[] = {"loadfile", path, NULL};

        mpv_command(ctx, cmd);

	return 0;
}

static int audio_decoder_track_ctrl_mpv(enum audio_decoder_ctrl ctrl)
{
	int val = 0;

	switch (ctrl) {
	case AUDIO_DECODER_PLAY:
		val = 0;
	break;
	case AUDIO_DECODER_PAUSE:
		val = 1;
	break;
	}

	mpv_set_property(ctx, "pause", MPV_FORMAT_FLAG, &val);

	return 0;
}

static void decode_metadata(mpv_event_property *prop)
{
	mpv_node *node = prop->data;
	const char *artist = NULL;
	const char *album = NULL;
	const char *title = NULL;
	int i;

	GP_DEBUG(2, "Track Metadata: %i", node->u.list->num);

	for (i = 0; i < node->u.list->num; i++) {
		mpv_node *val = &node->u.list->values[i];
		const char *key = node->u.list->keys[i];

		if (val->format != MPV_FORMAT_STRING) {
			GP_WARN("Wrong metadata value format?!");
			continue;
		}

		if (!strcmp(key, "artist"))
			artist = val->u.string;
		else if (!strcmp(key, "album"))
			album = val->u.string;
		else if (!strcmp(key, "title"))
			title = val->u.string;

		GP_DEBUG(3, "Medatada %s %s", key, val->u.string);
	}

	audio_decoder_track_info(artist, album, title);
}

static unsigned long audio_decoder_tick_mpv(void)
{
	for (;;) {
		mpv_event *event = mpv_wait_event(ctx, 0);

		if (event->event_id == MPV_EVENT_NONE)
			break;

		if (event->event_id == MPV_EVENT_SHUTDOWN) {
			audio_decoder_track_finished();
			break;
		}

		if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
			mpv_event_property *prop = (mpv_event_property *)event->data;

			GP_DEBUG(4, "MPV Event: %s %s",
			         mpv_event_name(event->event_id), prop->name);

			if (!strcmp(prop->name, "time-pos")) {
				if (prop->format == MPV_FORMAT_DOUBLE)
					audio_decoder_track_pos(*(double *)prop->data * 1000 + 0.5);
			} else if (!strcmp(prop->name, "duration")) {
				if (prop->format == MPV_FORMAT_DOUBLE)
					audio_decoder_track_duration(*(double *)prop->data * 1000 + 0.5);
				else
					audio_decoder_track_duration(0);
			} else if (!strcmp(prop->name, "metadata")) {
				if (prop->format == MPV_FORMAT_NODE)
					decode_metadata(prop);
			}
		} else {
			GP_DEBUG(4, "MPV Event: %s", mpv_event_name(event->event_id));
		}
	}

	return 100;
}

static int audio_decoder_track_seek_mpv(long seek_ms)
{
	double time_pos = (double)seek_ms/1000;

	mpv_set_property(ctx, "time-pos", MPV_FORMAT_DOUBLE, &time_pos);

	return 0;
}

unsigned long audio_decoder_softvol_mpv(enum audio_decoder_softvol_op op, unsigned long vol)
{
	double dvol;

	switch (op) {
	case AUDIO_DECODER_SOFTVOL_SET:
		dvol = vol;
		mpv_set_property(ctx, "volume", MPV_FORMAT_DOUBLE, &dvol);
	break;
	case AUDIO_DECODER_SOFTVOL_GET:
		mpv_get_property(ctx, "volume", MPV_FORMAT_DOUBLE, &dvol);
		return dvol;
	break;
	}

	return 0;
}

static const struct audio_decoder_ops audio_decoder_ops_mpv = {
	.track_load = audio_decoder_track_load_mpv,
	.track_ctrl = audio_decoder_track_ctrl_mpv,
	.track_seek = audio_decoder_track_seek_mpv,
	.softvol = audio_decoder_softvol_mpv,
	.tick = audio_decoder_tick_mpv,
};

const struct audio_decoder_ops *audio_decoder_mpv(const struct audio_decoder_callbacks *cbs)
{
	ctx = mpv_create();

	if (!ctx)
		return NULL;

	/* Disable video since we do not show it anywhere */
	mpv_set_option_string(ctx, "vid", "no");
	/* Enable metadata, duration and time position events */
	mpv_observe_property(ctx, 0, "metadata", MPV_FORMAT_NODE);
	mpv_observe_property(ctx, 0, "duration", MPV_FORMAT_DOUBLE);
	mpv_observe_property(ctx, 0, "time-pos", MPV_FORMAT_DOUBLE);
	/* Make sure mpv softvolume max matches our softvolume max */
	double dvol = AUDIO_DECODER_SOFTVOL_MAX;
	mpv_set_property(ctx, "volume-max", MPV_FORMAT_DOUBLE, &dvol);

	mpv_initialize(ctx);

	audio_decoder_cbs = cbs;

	return &audio_decoder_ops_mpv;
}
