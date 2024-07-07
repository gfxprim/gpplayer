//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <string.h>
#include <core/gp_debug.h>

#include "config.h"
#include "audio_decoder.h"

const struct audio_decoder audio_decoders[] = {
#ifdef HAVE_MPV_CLIENT_H
	{.name = "mpv", .init = audio_decoder_mpv},
#endif
#ifdef HAVE_MPG123_H
	{.name = "mpg123", .init = audio_decoder_mpg123},
#endif
	{}
};

const struct audio_decoder_ops *audio_decoder_init(const char *name, const struct audio_decoder_callbacks *cbs)
{
	unsigned int i;

	if (!name || !name[0])
		return audio_decoders[0].init(cbs);

	for (i = 0; audio_decoders[i].name; i++) {
		if (!strcmp(name, audio_decoders[i].name))
			return audio_decoders[i].init(cbs);
	}

	GP_WARN("Audio decoder '%s' not available falling back to '%s'",
		name, audio_decoders[0].name);

	return audio_decoders[0].init(cbs);
}

__attribute__((weak))
const struct audio_decoder_ops *audio_decoder_mpg123(const struct audio_decoder_callbacks *cbs)
{
	(void) cbs;
	return NULL;
}

__attribute__((weak))
const struct audio_decoder_ops *audio_decoder_mpv(const struct audio_decoder_callbacks *cbs)
{
	(void) cbs;
	return NULL;
}
