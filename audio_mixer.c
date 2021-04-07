//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2021 Cyril Hrubis <metan@ucw.cz>

 */

#include <core/gp_debug.h>
#include "audio_mixer.h"

static int mixer_poll_callback(struct gp_fd *self, struct pollfd *pfd)
{
	(void) pfd;
	struct audio_mixer *mixer = self->priv;

	snd_mixer_handle_events(mixer->mixer);

	return 0;
}

static void register_mixer_fds(struct audio_mixer *self)
{
	int i, nfds = snd_mixer_poll_descriptors_count(self->mixer);
	struct pollfd pfds[nfds];

	if (snd_mixer_poll_descriptors(self->mixer, pfds, nfds) < 0) {
		GP_WARN("Can't get mixer poll descriptors");
		return;
	}

	for (i = 0; i < nfds; i++)
		gp_fds_add(self->fds, pfds[i].fd, pfds[i].events, mixer_poll_callback, self);
}

static void unregister_mixer_fds(struct audio_mixer *self)
{
	int i, nfds = snd_mixer_poll_descriptors_count(self->mixer);
	struct pollfd pfds[nfds];

	if (snd_mixer_poll_descriptors(self->mixer, pfds, nfds) < 0) {
		GP_WARN("Can't get mixer poll descriptors");
		return;
	}

	for (i = 0; i < nfds; i++)
		gp_fds_rem(self->fds, pfds[i].fd);
}

static snd_mixer_t *mixer_open(const char *alsa_device)
{
	int ret;
	snd_mixer_t *mixer;

	struct snd_mixer_selem_regopt selem_regopt = {
		.ver = 1,
		.abstract = SND_MIXER_SABSTRACT_NONE,
		.device = alsa_device,
	};

	if ((ret = snd_mixer_open(&mixer, 0)) < 0) {
		GP_WARN("Failed to open alsa mixer '%s': %s",
		        alsa_device, snd_strerror(ret));
		return NULL;
	}

	if ((ret = snd_mixer_selem_register(mixer, &selem_regopt, NULL)) < 0) {
		GP_WARN("snd_mixer_selem_register(): %s", snd_strerror(ret));
		goto fail;
	}

	if ((ret = snd_mixer_load(mixer)) < 0) {
		GP_WARN("snd_mixer_load(): %s", snd_strerror(ret));
		goto fail;
	}

	return mixer;
fail:
	snd_mixer_close(mixer);
	return NULL;
}

static snd_mixer_elem_t *find_mixer_master(snd_mixer_t *mixer)
{
	snd_mixer_elem_t *elem;

	for (elem = snd_mixer_first_elem(mixer); elem; elem = snd_mixer_elem_next(elem)) {
		if (!snd_mixer_selem_has_playback_volume(elem))
			continue;

		if (strcmp(snd_mixer_selem_get_name(elem), "Master"))
			continue;

		break;
	}

	return elem;
}

static int mixer_volume_callback(snd_mixer_elem_t *elem, unsigned int mask)
{
	struct audio_mixer *self = snd_mixer_elem_get_callback_private(elem);

	if (!self->master_volume_callback)
		return 0;

	//TODO: mask

	long volume;
	int mute;

	if (self->use_db)
		snd_mixer_selem_get_playback_dB(elem, 0, &volume);
	else
		snd_mixer_selem_get_playback_volume(elem, 0, &volume);

	snd_mixer_selem_get_playback_switch(elem, 0, &mute);

	self->master_volume_callback(self, volume, !mute);

	return 0;
}

int audio_mixer_init(struct audio_mixer *self, const char *device)
{
	self->mixer = mixer_open(device);
	self->master_volume = find_mixer_master(self->mixer);

	register_mixer_fds(self);

	snd_mixer_elem_set_callback_private(self->master_volume, self);
	snd_mixer_elem_set_callback(self->master_volume, mixer_volume_callback);

	self->use_db = 1;
	snd_mixer_selem_get_playback_dB_range(self->master_volume,
	                                      &self->master_volume_min,
	                                      &self->master_volume_max);

	if (self->master_volume_min == self->master_volume_max) {
		self->use_db = 0;
		snd_mixer_selem_get_playback_volume_range(self->master_volume,
	                                                  &self->master_volume_min,
	                                                  &self->master_volume_max);
	}

	if (self->master_volume_callback)
		mixer_volume_callback(self->master_volume, 0);

	return 0;
}

void audio_mixer_set_master_volume(struct audio_mixer *self, long volume)
{
	if (self->use_db)
		snd_mixer_selem_set_playback_dB_all(self->master_volume, volume, 1);
	else
		snd_mixer_selem_set_playback_volume_all(self->master_volume, volume);

	mixer_volume_callback(self->master_volume, 0);
}

void audio_mixer_set_master_mute(struct audio_mixer *self, int mute)
{
	snd_mixer_selem_set_playback_switch_all(self->master_volume, !mute);

	mixer_volume_callback(self->master_volume, 0);
}

void audio_mixer_exit(struct audio_mixer *self)
{
	unregister_mixer_fds(self);

	snd_mixer_close(self->mixer);
}
