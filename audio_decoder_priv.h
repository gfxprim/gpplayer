//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef AUDIO_DECODER_PRIV_H
#define AUDIO_DECODER_PRIV_H

#include "audio_decoder.h"

static inline void audio_decoder_track_info(const char *artist, const char *album, const char *title)
{
	if (!audio_decoder_cbs || !audio_decoder_cbs->track_info)
		return;

	audio_decoder_cbs->track_info(artist, album, title);
}

static inline void audio_decoder_track_duration(long duration_ms)
{
	if (!audio_decoder_cbs || !audio_decoder_cbs->track_duration)
		return;

	audio_decoder_cbs->track_duration(duration_ms);
}

static inline void audio_decoder_track_art(const void *buf, size_t buf_len)
{
	if (!audio_decoder_cbs || !audio_decoder_cbs->track_art)
		return;

	audio_decoder_cbs->track_art(buf, buf_len);
}

static inline void audio_decoder_track_pos(long offset_ms)
{
	if (!audio_decoder_cbs || !audio_decoder_cbs->track_pos)
		return;

	audio_decoder_cbs->track_pos(offset_ms);
}

static inline void audio_decoder_track_finished(void)
{
	if (!audio_decoder_cbs || !audio_decoder_cbs->track_duration)
		return;

	audio_decoder_cbs->track_duration(0);
}

#endif /* AUDIO_DECODER_H */
