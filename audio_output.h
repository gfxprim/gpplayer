//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2020 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef AUDIO_OUTPUT_H__
#define AUDIO_OUTPUT_H__

#include <alsa/asoundlib.h>
#include <stdint.h>

#define AUDIO_DEVICE_DEFAULT "default"

enum audio_format {
	AUDIO_FORMAT_S16LE,
	AUDIO_FORMAT_S32LE,
};

struct audio_output {
	snd_pcm_t *playback_handle;
	snd_pcm_hw_params_t *hw_params;

	/* Current output settings */
	enum audio_format fmt;
	uint8_t channels;
	unsigned int sample_rate;
};

#define MW_ALSA_BUF_SIZE_TO_SAMPLES(alsa_out, buf_size) \
	(buf_size/((alsa_out)->channels * audio_format_size((alsa_out)->fmt)))

struct audio_output *audio_output_create(const char *alsa_device,
                                         uint8_t channels,
				         enum audio_format fmt,
					 unsigned int sample_rate);

int audio_output_start(struct audio_output *self);

int audio_output_stop(struct audio_output *self);

int audio_output_setup(struct audio_output *self,
                       uint8_t channels,
                       enum audio_format fmt,
                       unsigned int sample_rate);

unsigned int audio_format_size(enum audio_format fmt);

void audio_output_destroy(struct audio_output *self);

int audio_output_write(struct audio_output *self, void *buf,
                       unsigned int samples);

#endif /* AUDIO_OUTPUT_H__ */
