//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <mpg123.h>
#include <core/gp_common.h>
#include <core/gp_debug.h>

static const struct audio_decoder_callbacks *audio_decoder_cbs;

#include "audio_output.h"
#include "audio_decoder_priv.h"

static struct ad_mpg123 {
	mpg123_handle *handle;
	struct audio_output *out;
	long rate;
} ad_mpg123;

static enum audio_format convert_fmt(int encoding)
{
	switch (encoding) {
	case MPG123_ENC_SIGNED_16:
		return AUDIO_FORMAT_S16;
	break;
	case MPG123_ENC_SIGNED_32:
		return AUDIO_FORMAT_S32;
	break;
	default:
		printf("format %0x\n", encoding);
		return -1;
	}
}

static int audio_decoder_track_load_mpg123(const char *name)
{
	int ret;
	mpg123_handle *mh = ad_mpg123.handle;
	struct audio_output *out = ad_mpg123.out;

	if ((ret = mpg123_open(mh, name)) != MPG123_OK) {
		GP_WARN("Failed to open '%s': %s",
		        name, mpg123_plain_strerror(ret));
		return 1;
	}

	// Setup alsa output
	long rate;
	int channels;
	int encoding;

	if ((ret = mpg123_getformat(mh, &rate, &channels, &encoding)) != MPG123_OK) {
		GP_WARN("Failed to get format: %s", mpg123_plain_strerror(ret));
		return 1;
	}

	ad_mpg123.rate = rate;

	if (audio_output_setup(out, channels, convert_fmt(encoding), rate)) {
		GP_WARN("Failed to set output format");
		return 1;
	}

	// Now id3 tags
	mpg123_id3v1 *v1 = NULL;
	mpg123_id3v2 *v2 = NULL;

	if ((ret = mpg123_id3(mh, &v1, &v2))) {
		GP_DEBUG(1, "Failed to fetch id3 tags: %s",
		            mpg123_plain_strerror(ret));
	}

	// Do scan to compute length correctly
//	if (config.scan_duration)
		mpg123_scan(mh);

	long duration = 1000.0 * (double)mpg123_length(mh) / rate + 0.5;

	audio_decoder_track_duration(duration);

	if (v2) {
		audio_decoder_track_info(v2->artist ? v2->artist->p : NULL,
				         v2->album ? v2->album->p : NULL,
				         v2->title ? v2->title->p : NULL);

		size_t i;

		for (i = 0; i < v2->pictures; i++)
			audio_decoder_track_art(v2->picture[i].data, v2->picture[i].size);

		return 0;
	}

	if (v1) {
		audio_decoder_track_info(v1->artist, v1->album, v1->title);
		return 0;
	}

	audio_decoder_track_info(NULL, NULL, NULL);

	return 0;
}

static int audio_decoder_track_ctrl_mpg123(enum audio_decoder_ctrl ctrl)
{
	switch (ctrl) {
	case AUDIO_DECODER_PLAY:
		audio_output_start(ad_mpg123.out);
	break;
	case AUDIO_DECODER_PAUSE:
		audio_output_stop(ad_mpg123.out);
	break;
	}

	return 0;
}

static unsigned long audio_decoder_tick_mpg123(void)
{
	size_t size;
	mpg123_handle *mh = ad_mpg123.handle;
	struct audio_output *out = ad_mpg123.out;
	int avail = audio_buf_avail(out);
	unsigned char buf[avail];
	int ret;
	static unsigned long tick_ms = 1;

	if (avail < 1024) {
		tick_ms = GP_MIN(100u, tick_ms * 2);
		GP_DEBUG(1, "Autotune timer tick to %lu (avail=%i)", tick_ms, avail);
		return tick_ms;
	}

	if (avail > 8196) {
		tick_ms = GP_MAX(1u, tick_ms/2);
		GP_DEBUG(1, "Autotune timer tick to %lu (avail=%i)", tick_ms, avail);
	}

	ret = mpg123_read(mh, buf, sizeof(buf), &size);

	if (ret == MPG123_DONE)
		audio_decoder_track_finished();
	else
		audio_output_write(out, buf, AUDIO_BUFSIZE_TO_SAMPLES(out, size));

	long off = 1000.0 * (double)mpg123_tell(mh) / out->sample_rate + 0.5;

	audio_decoder_track_pos(off);

	return tick_ms;
}

static int audio_decoder_track_seek_mpg123(long seek_ms)
{
	mpg123_seek(ad_mpg123.handle, seek_ms * ad_mpg123.rate / 1000, SEEK_SET);
	return 0;
}

unsigned long audio_decoder_softvol_mpg123(enum audio_decoder_softvol_op op, unsigned long vol)
{
	double ret;

	switch (op) {
	case AUDIO_DECODER_SOFTVOL_SET:
		mpg123_volume(ad_mpg123.handle, 1.0 * vol / 100);
	break;
	case AUDIO_DECODER_SOFTVOL_GET:
		mpg123_getvolume(ad_mpg123.handle, &ret, NULL, NULL);
		return 100 * ret;
	break;
	}

	return 0;
}

static const struct audio_decoder_ops audio_decoder_ops_mpg123 = {
	.track_load = audio_decoder_track_load_mpg123,
	.track_ctrl = audio_decoder_track_ctrl_mpg123,
	.track_seek = audio_decoder_track_seek_mpg123,
	.softvol = audio_decoder_softvol_mpg123,
	.tick = audio_decoder_tick_mpg123,
};

const struct audio_decoder_ops *audio_decoder_mpg123(const struct audio_decoder_callbacks *cbs)
{
	mpg123_pars *mp;
	int res;

	if ((res = mpg123_init()) != MPG123_OK) {
		GP_WARN("Failed to initalize mpg123: %s",
		           mpg123_plain_strerror(res));
		goto err0;
	}

	mp = mpg123_new_pars(&res);

	if (!mp) {
		GP_WARN("Failed to get mpg123 parameters: %s",
		           mpg123_plain_strerror(res));
		goto err1;
	}

	ad_mpg123.handle = mpg123_new(NULL, &res);

	if (!ad_mpg123.handle) {
		GP_WARN("Failed to create mpg123 handle: %s",
		           mpg123_plain_strerror(res));
		goto err1;
	}

	mpg123_param(ad_mpg123.handle, MPG123_ADD_FLAGS, MPG123_PICTURE, 1.0);

	ad_mpg123.out = audio_output_create(AUDIO_DEVICE_DEFAULT, 2, AUDIO_FORMAT_S16, 48000);
	if (!ad_mpg123.out) {
		GP_WARN("Failed to initialize audio output");
		goto err1;
	}

	audio_decoder_cbs = cbs;

	return &audio_decoder_ops_mpg123;
err1:
	mpg123_exit();
err0:
	return NULL;
}
