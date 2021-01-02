//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2020 Cyril Hrubis <metan@ucw.cz>

 */

#include <mpg123.h>
#include <loaders/gp_loaders.h>
#include <filters/gp_resize.h>
#include <widgets/gp_widgets.h>

#include "audio_output.h"

static void *uids;

mpg123_handle *init_mpg123(void)
{
	mpg123_pars *mp;
	mpg123_handle *mh;
	int res;

	if ((res = mpg123_init()) != MPG123_OK) {
		GP_WARN("Failed to initalize mpg123: %s",
		           mpg123_plain_strerror(res));
		goto err0;
	}

	mp = mpg123_new_pars(&res);

	if (mp == NULL) {
		GP_WARN("Failed to get mpg123 parameters: %s",
		           mpg123_plain_strerror(res));
		goto err1;
	}

	mh = mpg123_new(NULL, &res);

	if (mh == NULL) {
		GP_WARN("Failed to create mpg123 handle: %s",
		           mpg123_plain_strerror(res));
		goto err1;
	}

	return mh;
err1:
	mpg123_exit();
err0:
	return NULL;
}

static enum audio_format convert_fmt(int encoding)
{
	//TODO endianity?
	switch (encoding) {
	case MPG123_ENC_SIGNED_16:
		return AUDIO_FORMAT_S16LE;
	break;
	case MPG123_ENC_SIGNED_32:
		return AUDIO_FORMAT_S32LE;
	break;
	default:
		printf("format %0x\n", encoding);
		return -1;
	}
}

struct player_tracks {
	unsigned int tracks;
	unsigned int cur_track;
	const char **paths;
	struct audio_output *out;
	mpg123_handle *mh;
	long rate;
	int playing;
};

struct player_tracks tracks;

struct info_widgets {
	gp_widget *album;
	gp_widget *artist;
	gp_widget *track;
	gp_widget *playback;
	gp_widget *cover_art;
} info_widgets;

static void set_info(const char *artist, const char *album, const char *track)
{
	GP_DEBUG(1, "Track name '%s' Album name '%s' Artist name '%s'",
	            track, album, artist);

	if (album)
		gp_widget_label_set(info_widgets.album, album);

	if (artist)
		gp_widget_label_set(info_widgets.artist, artist);

	if (track)
		gp_widget_label_set(info_widgets.track, track);
}

static void show_album_art(unsigned char *data, size_t size)
{
	gp_io *io = gp_io_mem(data, size, NULL);
	gp_pixmap *p;

	p = gp_read_image(io, NULL);

	if (!p)
		return;

	gp_widget *cover_art = info_widgets.cover_art;

	float rat = GP_MIN(1.00 * cover_art->h / p->h,
	                   1.00 * cover_art->w / p->w);

	if (cover_art->pixmap->pixmap) {
		gp_pixmap *resized = gp_filter_resize_alloc(p, p->w * rat, p->h * rat, GP_INTERP_LINEAR_LF_INT, NULL);
		gp_blit(resized, 0, 0, cover_art->w, cover_art->h, cover_art->pixmap->pixmap, 0, 0);
		gp_pixmap_free(resized);
	}

	gp_widget_redraw(cover_art);

	gp_pixmap_free(p);
	gp_io_close(io);
}

int load_track(struct audio_output *out, mpg123_handle *mh, const char *name)
{
	int ret;

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

	tracks.rate = rate;

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

	// duration = length / bitrate
	uint32_t duration = (double)mpg123_length(mh) / rate + 0.5;
	gp_widget_pbar_set_max(info_widgets.playback, duration);
	gp_widget_pbar_set(info_widgets.playback, 0);

	set_info("Unknown", "Unknown", "Unknown");

	if (v2 != NULL) {
		set_info(v2->artist ? v2->artist->p : NULL,
			 v2->album ? v2->album->p : NULL,
			 v2->title ? v2->title->p : NULL);

		size_t i;

		for (i = 0; i < v2->pictures; i++) {
			show_album_art(v2->picture[i].data, v2->picture[i].size);
		}

		return 0;
	}

	if (v1 != NULL) {
		set_info(v1->artist, v1->album, v1->title);
		return 0;
	}

	return 0;
}

int button_next_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	tracks.cur_track = (tracks.cur_track + 1) % tracks.tracks;

	load_track(tracks.out, tracks.mh, tracks.paths[tracks.cur_track]);

	return 1;
}

static gp_pixmap *alloc_backing_pixmap(gp_widget_event *ev)
{
	gp_pixmap *p;
	gp_widget *pix = ev->self;

	p = gp_pixmap_alloc(pix->w, pix->h, ev->ctx->pixel_type);
	if (p)
		gp_fill(p, ev->ctx->bg_color);

	return p;
}

int pixmap_event(gp_widget_event *ev)
{
	switch (ev->type) {
	case GP_WIDGET_EVENT_RESIZE:
		gp_pixmap_free(ev->self->pixmap->pixmap);
		ev->self->pixmap->pixmap = alloc_backing_pixmap(ev);
	break;
	default:
	break;
	}

	return 0;
}

int button_prev_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	tracks.cur_track = tracks.cur_track > 0 ?
	                   tracks.cur_track - 1 :
			   tracks.tracks - 1;

	load_track(tracks.out, tracks.mh, tracks.paths[tracks.cur_track]);

	return 1;
}

int button_play_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (tracks.playing == 1)
		return 1;

	tracks.playing = 1;
	audio_output_start(tracks.out);

	return 1;
}

int button_pause_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (tracks.playing == 0)
		return 1;

	tracks.playing = 0;
	audio_output_stop(tracks.out);

	return 1;
}

/*
static void seek_callback(struct MW_Widget *self)
{
	uint32_t val = MW_WidgetUIntValGet(self);

	GP_DEBUG(1, "Seeking to %u min %u sec", val/60, val%60);

	mpg123_seek(tracks.mh, val * tracks.rate, SEEK_SET);
}
*/

int main(int argc, char *argv[])
{
	struct audio_output *out;
	mpg123_handle *mh;

	gp_widget *layout = gp_widget_layout_json("gpplayer.json", &uids);

	info_widgets.artist = gp_widget_by_uid(uids, "artist", GP_WIDGET_LABEL);
	info_widgets.album = gp_widget_by_uid(uids, "album", GP_WIDGET_LABEL);
	info_widgets.track = gp_widget_by_uid(uids, "track", GP_WIDGET_LABEL);
	info_widgets.playback = gp_widget_by_uid(uids, "playback", GP_WIDGET_PROGRESSBAR);
	info_widgets.cover_art = gp_widget_by_uid(uids, "cover_art", GP_WIDGET_PIXMAP);

	gp_widget_event_unmask(info_widgets.cover_art, GP_WIDGET_EVENT_RESIZE);

	out = audio_output_create(AUDIO_DEVICE_DEFAULT, 2, AUDIO_FORMAT_S16LE, 48000);
	if (out == NULL) {
		GP_WARN("Failed to initalize alsa output");
		return 1;
	}

	mh = init_mpg123();
	if (mh == NULL)
		return 1;

	mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_PICTURE, 1.0);

	gp_widgets_getopt(&argc, &argv);

	tracks.tracks    = argc;
	tracks.cur_track = 0;
	tracks.paths     = (const char**)argv;
	tracks.out       = out;
	tracks.mh        = mh;
	tracks.playing   = 1;

	if (argc)
		load_track(out, mh, argv[0]);

	gp_widgets_layout_init(layout, "gpplayer");

	audio_output_start(out);

	for (;;) {
		size_t size;
		unsigned char buf[128];
		int ret;

		if (tracks.playing) {
			ret = mpg123_read(mh, buf, sizeof(buf), &size);

			uint32_t pos = (double)mpg123_tell(mh) / out->sample_rate + 0.5;

			gp_widget_pbar_set(info_widgets.playback, pos);

			// we are done, play next track
			if (ret == MPG123_DONE) {
				tracks.cur_track++;

				// album finished
				if (tracks.cur_track >= tracks.tracks) {
					tracks.cur_track--;
					tracks.playing = 0;
					audio_output_stop(out);
				} else {
					load_track(out, mh, tracks.paths[tracks.cur_track]);
				}

				continue;
			}

			audio_output_write(out, buf, MW_ALSA_BUF_SIZE_TO_SAMPLES(out, size));
		} else {
			usleep(100000);
		}

		if (gp_widgets_process_events(layout))
			exit(0);

		gp_widgets_redraw(layout);
	}

	return 0;
}
