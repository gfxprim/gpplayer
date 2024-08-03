//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <loaders/gp_loaders.h>
#include <filters/gp_resize.h>
#include <widgets/gp_widgets.h>

#include "playlist.h"
#include "audio_mixer.h"
#include "audio_output.h"
#include "audio_decoder.h"
#include "gpplayer_conf.h"

static gp_htable *uids;

static const struct audio_decoder_ops *ad_ops;

struct player_tracks {
	int playing;
};

static uint32_t playback_callback(gp_timer GP_UNUSED(*self))
{
	return audio_decoder_tick(ad_ops);
}

static gp_timer playback_timer = {
	.expires = 0,
	.callback = playback_callback,
	.id = "Playback",
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

static void track_info(const char *artist, const char *album, const char *track)
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

static void track_duration(long duration_ms)
{
	if (!duration_ms) {
		if (!playlist_next()) {
			tracks.playing = 0;
			gp_widgets_timer_rem(&playback_timer);
			audio_decoder_track_pause(ad_ops);
			return;
		}

		//TODO: playlist should call this
		gp_widget_redraw(info_widgets.playlist);

		audio_decoder_track_load(ad_ops, playlist_cur());

		return;
	}

	gp_widget_pbar_val_set(info_widgets.playback, 0);
	gp_widget_pbar_max_set(info_widgets.playback, duration_ms/1000);
}

static void track_pos(long offset_ms)
{
	gp_widget_pbar_val_set(info_widgets.playback, offset_ms/1000);
}

static void track_art(void *data, size_t size)
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

static void start_playback_timer(void)
{
	tracks.playing = 1;
	audio_decoder_track_play(ad_ops);
	playback_timer.expires = 0;
	gp_widgets_timer_ins(&playback_timer);
}

int button_next_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!playlist_next())
		return 0;

	//TODO: playlist should call this
	gp_widget_redraw(info_widgets.playlist);

	audio_decoder_track_load(ad_ops, playlist_cur());

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
	case GP_WIDGET_EVENT_COLOR_SCHEME:
		gp_fill(ev->self->pixmap->pixmap, ev->ctx->bg_color);
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

	//TODO: playlist shoudl call this
	gp_widget_redraw(info_widgets.playlist);

	audio_decoder_track_load(ad_ops, playlist_cur());

	return 1;
}

int button_play_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (tracks.playing == 1)
		return 1;

	start_playback_timer();
	audio_decoder_track_play(ad_ops);

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
	audio_decoder_track_pause(ad_ops);

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

	audio_decoder_track_load(ad_ops, playlist_cur());
	//TODO: playlist shoudl call this
	gp_widget_redraw(info_widgets.playlist);
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

	gp_dialog *dialog = gp_dialog_file_open_new(gpplayer_conf->last_dialog_path, &opts);

	if (gp_dialog_run(dialog) == GP_WIDGET_DIALOG_PATH) {
		playlist_add(gp_dialog_file_path(dialog));
		gpplayer_conf_last_dialog_path_set(gp_dialog_file_path(dialog));
	}

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

int playlist_repeat(gp_widget_event *ev)
{
	switch (ev->type) {
	case GP_WIDGET_EVENT_NEW:
		gp_widget_bool_set(ev->self, gpplayer_conf->playlist_repeat);
	break;
	case GP_WIDGET_EVENT_WIDGET:
		gpplayer_conf_playlist_repeat_set(gp_widget_bool_get(ev->self));
	break;
	}

	return 0;
}

int playlist_shuffle(gp_widget_event *ev)
{
	switch (ev->type) {
	case GP_WIDGET_EVENT_NEW:
		gp_widget_bool_set(ev->self, gpplayer_conf->playlist_shuffle);
	break;
	case GP_WIDGET_EVENT_WIDGET:
		gpplayer_conf_playlist_shuffle_set(gp_widget_bool_get(ev->self));
		playlist_shuffle_set(gp_widget_bool_get(ev->self));
	break;
	}

	return 0;
}

int seek_on_event(gp_widget_event *ev)
{
	uint64_t val = gp_widget_pbar_val_get(ev->self);

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	GP_DEBUG(1, "Seeking to %"PRIu64" min %"PRIu64" sec", val/60, val%60);

	audio_decoder_track_seek(ad_ops, 1000 * val);

	return 0;
}

static int app_handler(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_FREE)
		return 0;

	playlist_exit();
	gpplayer_conf_save();
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
	if (ev->type == GP_WIDGET_EVENT_NEW) {
		gp_widget_int_set(ev->self, 0, AUDIO_DECODER_SOFTVOL_MAX, gpplayer_conf->softvol);
		return 0;
	}

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	unsigned int max = ev->self->slider->max;
	unsigned int val = gp_widget_int_val_get(ev->self);

	audio_decoder_softvol_set(ad_ops, val);
	gpplayer_conf_softvol_set(val);

	if (!info_widgets.softvol_icon)
		return 0;

	int type;

	if (val < 1.00 * max / 3)
		type = GP_WIDGET_STOCK_SPEAKER_MIN;
	else if (val < 2.00 * max / 3)
		type = GP_WIDGET_STOCK_SPEAKER_MID;
	else
		type = GP_WIDGET_STOCK_SPEAKER_MAX;

	gp_widget_stock_type_set(info_widgets.softvol_icon, type);

	return 0;
}

static struct audio_decoder_callbacks ad_callbacks = {
	.track_info = track_info,
		.track_art = track_art,
	.track_duration = track_duration,
	.track_pos = track_pos,
};

gp_app_info app_info = {
	.name = "gpplayer",
	.desc = "A simple mp3 player",
	.version = "1.0",
	.license = "GPL-2.0-or-later",
	.url = "http://github.com/gfxprim/gpplayer",
	.authors = (gp_app_info_author []) {
		{.name = "Cyril Hrubis", .email = "metan@ucw.cz", .years = "2007-2024"},
		{}
	}
};

static void init_decoder(void)
{
	ad_ops = audio_decoder_init(gpplayer_conf->decoder, &ad_callbacks);

	/* Restore softvolume from config */
	audio_decoder_softvol_set(ad_ops, gpplayer_conf->softvol);
}

int main(int argc, char *argv[])
{
	int i;

	struct audio_mixer mixer = {
		.master_volume_callback = mixer_volume_callback,
	};

	gpplayer_conf_load();

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

	gp_widget_events_unmask(info_widgets.cover_art, GP_WIDGET_EVENT_RESIZE |
	                                                GP_WIDGET_EVENT_COLOR_SCHEME);

	gp_widget *volume = gp_widget_by_uid(uids, "volume", GP_WIDGET_SLIDER);

	volume->priv = &mixer;
	mixer.priv = volume;

	audio_mixer_init(&mixer, AUDIO_DEVICE_DEFAULT);

	for (i = 0; i < (int)mixer.poll_fds_cnt; i++)
		gp_widget_poll_add(&mixer.poll_fds[i]);

	gp_widgets_getopt(&argc, &argv);

	if (argc)
		playlist_init(NULL);
	else
		playlist_init("gpapps/gpplayer/playlist.txt");

	playlist_list();

	gp_app_on_event_set(app_handler);

	init_decoder();

	for (i = 0; i < argc; i++)
		playlist_add(argv[i]);

	if (playlist_cur()) {
		tracks.playing = 1;
		audio_decoder_track_load(ad_ops, playlist_cur());
		gp_widgets_timer_ins(&playback_timer);
	}

	gp_widgets_main_loop(layout, NULL, 0, NULL);

	return 0;
}
