//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (C) 2007-2020 Cyril Hrubis <metan@ucw.cz>

 */

#include <mpg123.h>
#include <loaders/gp_loaders.h>
#include <filters/gp_resize.h>
#include <widgets/gp_widgets.h>

#include "playlist.h"
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
	gp_widget *playlist;
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

static uint32_t playback_callback(gp_timer *self)
{
	size_t size;
	int avail = audio_buf_avail(tracks.out);
	unsigned char buf[avail];
	int ret;

	if (avail < 256)
		return 10;

	ret = mpg123_read(tracks.mh, buf, sizeof(buf), &size);

	uint32_t pos = (double)mpg123_tell(tracks.mh) / tracks.out->sample_rate + 0.5;

	gp_widget_pbar_set(info_widgets.playback, pos);

	/* we are done, play next track */
	if (ret == MPG123_DONE) {
		if (playlist_next()) {
			load_track(tracks.out, tracks.mh, playlist_cur());
		} else {
			tracks.playing = 0;
			audio_output_stop(tracks.out);
		}
	}

	audio_output_write(tracks.out, buf, AUDIO_BUFSIZE_TO_SAMPLES(tracks.out, size));
	return 10;
}

static gp_timer playback_timer = {
	.expires = 10,
	.period = 10,
	.callback = playback_callback,
	.id = "Playback",
};

int button_next_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!playlist_next())
		return 0;

	load_track(tracks.out, tracks.mh, playlist_cur());

	return 0;
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

	if (!playlist_prev())
		return 0;

	load_track(tracks.out, tracks.mh, playlist_cur());

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
	gp_widgets_timer_ins(&playback_timer);

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
	gp_widgets_timer_rem(&playback_timer);

	return 1;
}

int playlist_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!playlist_set(ev->self->tbl->selected_row))
		return 0;

	load_track(tracks.out, tracks.mh, playlist_cur());
	if (!tracks.playing) {
		tracks.playing = 1;
		audio_output_start(tracks.out);
		gp_widgets_timer_ins(&playback_timer);
	}

	return 0;
}

int button_playlist_rem(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!info_widgets.playlist->tbl->row_selected)
		return 0;

	playlist_rem(info_widgets.playlist->tbl->selected_row, 1);
	gp_widget_table_refresh(info_widgets.playlist);
	return 0;
}

int button_playlist_add(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gp_widget_dialog *dialog = gp_widget_dialog_file_open_new(NULL);

	if (gp_widget_dialog_run(dialog) == GP_WIDGET_DIALOG_PATH)
		playlist_add(gp_widget_dialog_file_open_path(dialog));

	gp_widget_dialog_free(dialog);

	return 0;
}

int button_playlist_move_up(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!info_widgets.playlist->tbl->row_selected)
		return 0;

	if (playlist_move_up(info_widgets.playlist->tbl->selected_row))
		info_widgets.playlist->tbl->selected_row--;

	gp_widget_table_refresh(info_widgets.playlist);
	return 0;
}

int button_playlist_move_down(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!info_widgets.playlist->tbl->row_selected)
		return 0;

	if (playlist_move_down(info_widgets.playlist->tbl->selected_row))
		info_widgets.playlist->tbl->selected_row++;

	gp_widget_table_refresh(info_widgets.playlist);
	return 0;
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
	int i;

	gp_widget *layout = gp_widget_layout_json("gpplayer.json", &uids);

	info_widgets.artist = gp_widget_by_uid(uids, "artist", GP_WIDGET_LABEL);
	info_widgets.album = gp_widget_by_uid(uids, "album", GP_WIDGET_LABEL);
	info_widgets.track = gp_widget_by_uid(uids, "track", GP_WIDGET_LABEL);
	info_widgets.playback = gp_widget_by_uid(uids, "playback", GP_WIDGET_PROGRESSBAR);
	info_widgets.cover_art = gp_widget_by_uid(uids, "cover_art", GP_WIDGET_PIXMAP);
	info_widgets.playlist = gp_widget_by_uid(uids, "playlist", GP_WIDGET_TABLE);

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

	if (argc)
		playlist_init(NULL);
	else
		playlist_init("gpapps/gpplayer/playlist.txt");

	for (i = 0; i < argc; i++)
		playlist_add(argv[i]);

	playlist_list();

	tracks.out = out;
	tracks.mh = mh;

	if (playlist_cur()) {
		tracks.playing = 1;
		load_track(out, mh, playlist_cur());
	}

	audio_output_start(out);
	gp_widgets_timer_ins(&playback_timer);
	gp_widgets_main_loop(layout, "gpplayer", NULL, 0, NULL);

	return 0;
}
