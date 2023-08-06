//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2022 Cyril Hrubis <metan@ucw.cz>

 */

#include <mpg123.h>
#include <loaders/gp_loaders.h>
#include <filters/gp_resize.h>
#include <widgets/gp_widgets.h>

#include "playlist.h"
#include "audio_mixer.h"
#include "audio_output.h"

static gp_htable *uids;

static mpg123_handle *init_mpg123(void)
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

struct player_tracks {
	struct audio_output *out;
	mpg123_handle *mh;
	long rate;
	int playing;
};

static struct player_tracks tracks;

struct info_widgets {
	gp_widget *album;
	gp_widget *artist;
	gp_widget *track;
	gp_widget *playback;
	gp_widget *cover_art;
	gp_widget *playlist;
	gp_widget *speaker_icon;
	gp_widget *softvol_icon;
	gp_widget *decoder_gain;
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
	gp_widget *cover_art = info_widgets.cover_art;
	gp_io *io;
	gp_pixmap *p;

	if (!cover_art->pixmap->pixmap)
		return;

	io = gp_io_mem(data, size, NULL);
	if (!io)
		return;

	p = gp_read_image(io, NULL);
	if (!p) {
		gp_io_close(io);
		return;
	}

	float rat = GP_MIN(1.00 * cover_art->h / p->h,
	                   1.00 * cover_art->w / p->w);

	gp_size rw = p->w * rat;
	gp_size rh = p->h * rat;

	gp_coord off_x = (cover_art->w - rw)/2;
	gp_coord off_y = (cover_art->h - rh)/2;

	gp_pixmap *resized = gp_filter_resize_alloc(p, rw, rh, GP_INTERP_LINEAR_LF_INT, NULL);
	gp_blit(resized, off_x, off_y, resized->w, resized->h, cover_art->pixmap->pixmap, 0, 0);
	gp_pixmap_free(resized);

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

	gp_widget_redraw(info_widgets.playlist);

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
	static unsigned int tick_ms = 1;

	if (avail < 1024) {
		tick_ms = GP_MIN(100u, tick_ms * 2);
		GP_DEBUG(1, "Autotune timer tick to %u (avail=%i)", tick_ms, avail);
		return tick_ms;
	}

	if (avail > 8196) {
		tick_ms = GP_MAX(1u, tick_ms/2);
		GP_DEBUG(1, "Autotune timer tick to %u (avail=%i)", tick_ms, avail);
	}

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
			return GP_TIMER_STOP;
		}
	}

	audio_output_write(tracks.out, buf, AUDIO_BUFSIZE_TO_SAMPLES(tracks.out, size));
	return tick_ms;
}

static gp_timer playback_timer = {
	.expires = 0,
	.callback = playback_callback,
	.id = "Playback",
};

static void start_playback_timer(void)
{
	tracks.playing = 1;
	audio_output_start(tracks.out);
	playback_timer.expires = 0;
	gp_widgets_timer_ins(&playback_timer);
}

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

	start_playback_timer();

	return 1;
}

int button_pause_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (tracks.playing == 0)
		return 1;

	tracks.playing = 0;
	gp_widgets_timer_rem(&playback_timer);
	audio_output_stop(tracks.out);

	return 1;
}

int playlist_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (ev->sub_type != GP_WIDGET_TABLE_TRIGGER)
		return 0;

	if (!playlist_set(ev->self->tbl->selected_row))
		return 0;

	load_track(tracks.out, tracks.mh, playlist_cur());
	if (!tracks.playing)
		start_playback_timer();

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

int button_playlist_clear(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	playlist_clear();

	gp_widget_table_refresh(info_widgets.playlist);
	return 0;
}

int button_playlist_add(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gp_dialog_file_opts opts = {
		.flags = GP_DIALOG_OPEN_FILE | GP_DIALOG_OPEN_DIR,
	};

	gp_dialog *dialog = gp_dialog_file_open_new(NULL, &opts);

	if (gp_dialog_run(dialog) == GP_WIDGET_DIALOG_PATH)
		playlist_add(gp_dialog_file_path(dialog));

	gp_dialog_free(dialog);

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

static int app_handler(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_FREE)
		return 0;

	playlist_exit();

	//stop audio output?

	return 1;
}

static void update_speaker_icon(int vol, int max, int mute)
{
	int type;

	if (!info_widgets.speaker_icon)
		return;

	if (vol < max/3)
		type = GP_WIDGET_STOCK_SPEAKER_MIN;
	else if (vol < 2 * (max/3))
		type = GP_WIDGET_STOCK_SPEAKER_MID;
	else
		type = GP_WIDGET_STOCK_SPEAKER_MAX;

	if (mute)
		type = GP_WIDGET_STOCK_SPEAKER_MUTE;

	gp_widget_stock_type_set(info_widgets.speaker_icon, type);
}

static void mixer_volume_callback(struct audio_mixer *mixer, long volume, int mute)
{
	long avol = volume - mixer->master_volume_min;
	long amax = mixer->master_volume_max - mixer->master_volume_min;
	gp_widget *self = mixer->priv;

	int vol = (avol * self->i->max + amax/2) / amax;

	update_speaker_icon(vol, self->i->max, mute);

	gp_widget_int_val_set(self, vol);
}

int audio_volume_set(gp_widget_event *ev)
{
	struct audio_mixer *mixer = ev->self->priv;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	long amax = mixer->master_volume_max - mixer->master_volume_min;
	int vol = gp_widget_int_val_get(ev->self);
	int max = ev->self->i->max;

	long avol = (vol * amax + max/2) / max + mixer->master_volume_min;

	audio_mixer_set_master_volume(mixer, avol);

	return 0;
}

int speaker_icon_ev(gp_widget_event *ev)
{
	struct audio_mixer *mixer = ev->self->priv;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (ev->self->stock->type == GP_WIDGET_STOCK_SPEAKER_MUTE)
		audio_mixer_set_master_mute(mixer, 0);
	else
		audio_mixer_set_master_mute(mixer, 1);

	return 0;
}

int set_softvol(gp_widget_event *ev)
{
	unsigned int max = ev->self->slider->max;
	unsigned int val = ev->self->slider->val;

	if (ev->type == GP_WIDGET_EVENT_NEW) {
		gp_widget_int_val_set(ev->self, max - max/6);
		return 0;
	}

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	double vol = 1.00 * val / (max - max/6);

	mpg123_volume(tracks.mh, vol);

	if (!info_widgets.softvol_icon)
		return 0;

	int type;

	if (vol < 1.00/3)
		type = GP_WIDGET_STOCK_SPEAKER_MIN;
	else if (vol < 2.00/3)
		type = GP_WIDGET_STOCK_SPEAKER_MID;
	else
		type = GP_WIDGET_STOCK_SPEAKER_MAX;

	gp_widget_stock_type_set(info_widgets.softvol_icon, type);

	return 0;
}

static gp_app_info app_info = {
	.name = "gpplayer",
	.desc = "A simple mp3 player",
	.version = "1.0",
	.license = "GPL-2.0-or-later",
	.url = "http://github.com/gfxprim/gpplayer",
	.authors = (gp_app_info_author []) {
		{.name = "Cyril Hrubis", .email = "metan@ucw.cz", .years = "2007-2022"},
		{}
	}
};

int main(int argc, char *argv[])
{
	struct audio_output *out;
	mpg123_handle *mh;
	int i;

	struct audio_mixer mixer = {
		.master_volume_callback = mixer_volume_callback,
	};

	gp_app_info_set(&app_info);

	gp_widget *layout = gp_app_layout_load("gpplayer", &uids);

	info_widgets.artist = gp_widget_by_uid(uids, "artist", GP_WIDGET_LABEL);
	info_widgets.album = gp_widget_by_uid(uids, "album", GP_WIDGET_LABEL);
	info_widgets.track = gp_widget_by_uid(uids, "track", GP_WIDGET_LABEL);
	info_widgets.playback = gp_widget_by_uid(uids, "playback", GP_WIDGET_PROGRESSBAR);
	info_widgets.cover_art = gp_widget_by_uid(uids, "cover_art", GP_WIDGET_PIXMAP);
	info_widgets.playlist = gp_widget_by_uid(uids, "playlist", GP_WIDGET_TABLE);
	info_widgets.speaker_icon = gp_widget_by_uid(uids, "speaker_icon", GP_WIDGET_STOCK);
	info_widgets.softvol_icon = gp_widget_by_uid(uids, "softvol_icon", GP_WIDGET_STOCK);
	info_widgets.decoder_gain = gp_widget_by_uid(uids, "gain", GP_WIDGET_STOCK);

	if (info_widgets.speaker_icon)
		info_widgets.speaker_icon->priv = &mixer;

	gp_widget_event_unmask(info_widgets.cover_art, GP_WIDGET_EVENT_RESIZE);

	gp_widget *volume = gp_widget_by_uid(uids, "volume", GP_WIDGET_SLIDER);

	volume->priv = &mixer;
	mixer.priv = volume;

	audio_mixer_init(&mixer, AUDIO_DEVICE_DEFAULT);

	out = audio_output_create(AUDIO_DEVICE_DEFAULT, 2, AUDIO_FORMAT_S16, 48000);
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

	gp_app_on_event_set(app_handler);

	for (i = 0; i < argc; i++)
		playlist_add(argv[i]);

	tracks.out = out;
	tracks.mh = mh;

	if (playlist_cur()) {
		tracks.playing = 1;
		load_track(out, mh, playlist_cur());
		audio_output_start(out);
		gp_widgets_timer_ins(&playback_timer);
	}

	gp_widgets_main_loop(layout, "gpplayer", NULL, 0, NULL);

	return 0;
}
