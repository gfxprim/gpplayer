//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2020 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef AUDIO_MIXER_H__
#define AUDIO_MIXER_H__

#include <utils/gp_fds.h>
#include <alsa/asoundlib.h>

struct audio_mixer {
	snd_mixer_t *mixer;
	struct gp_fds *fds;

	snd_mixer_elem_t *master_volume;
	long master_volume_min;
	long master_volume_max;

	int use_db:1;

	void (*master_volume_callback)(struct audio_mixer *mixer, long volume, int mute);
	void *priv;
};

int audio_mixer_init(struct audio_mixer *self, const char *device);

void audio_mixer_set_master_volume(struct audio_mixer *self, long volume);

void audio_mixer_set_master_mute(struct audio_mixer *self, int mute);

void audio_mixer_exit(struct audio_mixer *self);

#endif /* AUDIO_MIXER_H__ */
