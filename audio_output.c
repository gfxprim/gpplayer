//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2020 Cyril Hrubis <metan@ucw.cz>

 */

#include <core/gp_debug.h>
#include "audio_output.h"

#define BUFSIZE 512

static uint32_t convert_fmt(enum audio_format fmt)
{
	switch (fmt) {
	case AUDIO_FORMAT_S16LE:
		return SND_PCM_FORMAT_S16_LE;
	break;
	case AUDIO_FORMAT_S16BE:
		return SND_PCM_FORMAT_S16_BE;
	break;
	case AUDIO_FORMAT_S32LE:
		return SND_PCM_FORMAT_S32_LE;
	break;
	case AUDIO_FORMAT_S32BE:
		return SND_PCM_FORMAT_S32_BE;
	break;
	default:
		return SND_PCM_FORMAT_UNKNOWN;
	}
}

static const char *str_fmt(enum audio_format fmt)
{
	switch (fmt) {
	case AUDIO_FORMAT_S16LE:
		return "s16le";
	case AUDIO_FORMAT_S16BE:
		return "s16be";
	case AUDIO_FORMAT_S32LE:
		return "s32le";
	case AUDIO_FORMAT_S32BE:
		return "s32be";
	default:
		return "unknown";
	}
}

unsigned int audio_format_size(enum audio_format fmt)
{
	switch (fmt) {
	case AUDIO_FORMAT_S16LE:
	case AUDIO_FORMAT_S16BE:
		return 2;
	case AUDIO_FORMAT_S32LE:
	case AUDIO_FORMAT_S32BE:
		return 4;
	default:
		return 0;
	}
}

struct audio_output *audio_output_create(const char *alsa_device,
                                         uint8_t channels,
				         enum audio_format fmt,
                                         uint32_t sample_rate)
{
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *hw_params;

	GP_DEBUG(1, "Initializing alsa device '%s'", alsa_device);

	if (snd_pcm_open(&handle, alsa_device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		GP_WARN("Failed to open alsa output '%s'", alsa_device);
		return NULL;
	}

	snd_pcm_hw_params_malloc(&hw_params);
	snd_pcm_hw_params_any(handle, hw_params);
	snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, hw_params, convert_fmt(fmt));
	unsigned int rate = sample_rate;
	snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0);
	snd_pcm_hw_params_set_channels(handle, hw_params, channels);
	snd_pcm_hw_params_set_periods(handle, hw_params, 32, 0);
	snd_pcm_hw_params_set_period_size(handle, hw_params, BUFSIZE, 0);
	snd_pcm_hw_params(handle, hw_params);

	struct audio_output *new = malloc(sizeof(struct audio_output));

	if (new == NULL) {
		GP_WARN("Malloc failed :(");
		snd_pcm_close(handle);
		snd_pcm_hw_params_free(hw_params);
	}

	new->fmt         = fmt;
	new->channels    = channels;
	new->sample_rate = sample_rate;

	new->playback_handle = handle;
	new->hw_params = hw_params;

	return new;
}

int audio_output_start(struct audio_output *self)
{
	int ret;

	GP_DEBUG(1, "Starting alsa output");

	ret = snd_pcm_set_params(self->playback_handle, convert_fmt(self->fmt),
	                         SND_PCM_ACCESS_RW_INTERLEAVED,
				 self->channels, self->sample_rate, 1, 500000);
	if (ret < 0) {
		GP_WARN("snd_pcm_hw_params(): %s", snd_strerror(ret));
		return ret;
	}

	ret = snd_pcm_start(self->playback_handle);

	if (ret < 0)
		GP_WARN("snd_pcm_start(): %s", snd_strerror(ret));

	return ret;
}

int audio_output_stop(struct audio_output *self)
{
	int ret;

	GP_DEBUG(1, "Stopping alsa output");

	ret = snd_pcm_drop(self->playback_handle);

	if (ret < 0)
		GP_WARN("snd_pcm_drop(): %s", snd_strerror(ret));

	return ret;
}

int audio_output_setup(struct audio_output *self,
                       uint8_t channels,
                       enum audio_format fmt,
                       unsigned int sample_rate)
{
	snd_pcm_t *handle = self->playback_handle;
	int ret;

	GP_DEBUG(1, "Setting alsa output format "
	            "(%u channels, %u sample rate, %s format)",
	            channels, sample_rate, str_fmt(fmt));

	/* Nothing to be done */
	if (self->fmt == fmt &&
	    self->sample_rate == sample_rate &&
	    self->channels == channels) {
		return 0;
	}

	unsigned int rate = sample_rate;

	snd_pcm_hw_params_set_format(handle, self->hw_params, convert_fmt(fmt));
	snd_pcm_hw_params_set_rate_near(handle, self->hw_params, &rate, 0);
	snd_pcm_hw_params_set_channels(handle, self->hw_params, channels);

	ret = snd_pcm_hw_params(handle, self->hw_params);
	if (ret) {
		GP_WARN("snd_pcm_hw_params(handle, hw_params): %s",
			snd_strerror(ret));
	}

	ret = snd_pcm_set_params(handle, convert_fmt(fmt),
	                         SND_PCM_ACCESS_RW_INTERLEAVED,
				 channels, sample_rate, 1, 500000);

	if (ret) {
		GP_WARN("Failed to set audio format: %s",
		        snd_strerror(ret));
		return -1;
	}

	self->fmt = fmt;
	self->channels = channels;
	self->sample_rate = sample_rate;

	return 0;
}

unsigned int audio_buf_avail(struct audio_output *self)
{
	snd_pcm_sframes_t avail = snd_pcm_avail(self->playback_handle);

	if (avail < 0) {
		GP_WARN("snd_pcm_avail(): %s", snd_strerror(avail));
		snd_pcm_prepare(self->playback_handle);
		return 256;
	}

	return AUDIO_SAMPLES_TO_BUFSIZE(self, avail);
}

void audio_output_destroy(struct audio_output *self)
{
	GP_DEBUG(1, "Destroing ALSA output");
	snd_pcm_close(self->playback_handle);
	snd_pcm_hw_params_free(self->hw_params);
	free(self);
}

int audio_output_write(struct audio_output *self, void *buf,
                       unsigned int samples)
{
	int ret;

	if ((ret = snd_pcm_writei(self->playback_handle, buf, samples)) < 0) {
		snd_pcm_prepare(self->playback_handle);
		GP_WARN("Buffer underun");
	}

	return ret;
}
