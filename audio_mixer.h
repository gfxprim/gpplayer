//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2022 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef AUDIO_MIXER_H__
#define AUDIO_MIXER_H__

#include <utils/gp_poll.h>
#include <alsa/asoundlib.h>

struct audio_mixer {
	snd_mixer_t *mixer;

	snd_mixer_elem_t *master_volume;
	long master_volume_min;
	long master_volume_max;

	int use_db:1;

	void (*master_volume_callback)(struct audio_mixer *mixer, long volume, int mute);
	void *priv;

	/* Array of file descriptors with callbacks */
	size_t poll_fds_cnt;
	gp_fd *poll_fds;
};

int audio_mixer_init(struct audio_mixer *self, const char *device);

void audio_mixer_set_master_volume(struct audio_mixer *self, long volume);

void audio_mixer_set_master_mute(struct audio_mixer *self, int mute);

void audio_mixer_exit(struct audio_mixer *self);

#endif /* AUDIO_MIXER_H__ */
