//SPDX-License-Identifier: GPL-2.0-or-later
/*

   Copyright (C) 2007-2024 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include <stddef.h>

/**
 * @brief Maximum softvolume gain is 130%
 */
#define AUDIO_DECODER_SOFTVOL_MAX 130

/**
 * @brief Audio decoder control.
 */
enum audio_decoder_ctrl {
	/** @brief Resume playback after a pause. */
	AUDIO_DECODER_PLAY,
	/** @brief Pause playback. */
	AUDIO_DECODER_PAUSE,
};

/**
 * @brief Software volume control.
 */
enum audio_decoder_softvol_op {
	/** @brief Gets soft volume value. */
	AUDIO_DECODER_SOFTVOL_GET,
	/** @brief Sets soft volume value. */
	AUDIO_DECODER_SOFTVOL_SET,
};

/**
 * @brief Audio decoder options.
 *
 * This structure should be returned by the audio decoder init function.
 */
struct audio_decoder_ops {
	/**
	 * @brief Loads a new track.
	 *
	 * @param path A path to the file.
	 * @return Zero on success.
	 */
	int (*track_load)(const char *path);
	/**
	 * @brief Seeks in the current track.
	 *
	 * @param whence A seek whence.
	 * @param offset_ms An offset in miliseconds.
	 *
	 * @return Zero on success.
	 */
	int (*track_seek)(long offset_ms);
	/**
	 * @brief Playback control.
	 *
	 * @param playback A playback operation.
	 */
	int (*track_ctrl)(enum audio_decoder_ctrl ctrl);
	/**
	 * @brief Software volume control.
	 *
	 * @param vol A new volume for the set op.
	 *
	 * @return Zero for set, current volume for cur and maximal volume for max.
	 */
	unsigned long (*softvol)(enum audio_decoder_softvol_op op, unsigned long vol);
	/**
	 * @brief Processes events, fills buffers, etc.
	 *
	 * This has to be called regularly to process events, fill buffers, etc.
	 *
	 * @return An interval before next call to this function in miliseconds.
	 */
	unsigned long (*tick)(void);
};

static inline int audio_decoder_track_load(const struct audio_decoder_ops *ops, const char *path)
{
	return ops->track_load(path);
}

static inline unsigned long audio_decoder_tick(const struct audio_decoder_ops *ops)
{
	return ops->tick();
}

static inline void audio_decoder_track_pause(const struct audio_decoder_ops *ops)
{
	ops->track_ctrl(AUDIO_DECODER_PAUSE);
}

static inline void audio_decoder_track_play(const struct audio_decoder_ops *ops)
{
	ops->track_ctrl(AUDIO_DECODER_PLAY);
}

static inline void audio_decoder_track_seek(const struct audio_decoder_ops *ops, unsigned long offset_ms)
{
	ops->track_seek(offset_ms);
}

static inline unsigned long audio_decoder_softvol_max(const struct audio_decoder_ops *ops)
{
	return ops->softvol(AUDIO_DECODER_SOFTVOL_MAX, 0);
}

static inline unsigned long audio_decoder_softvol_get(const struct audio_decoder_ops *ops)
{
	return ops->softvol(AUDIO_DECODER_SOFTVOL_GET, 0);
}

static inline unsigned long audio_decoder_softvol_set(const struct audio_decoder_ops *ops, unsigned long vol)
{
	return ops->softvol(AUDIO_DECODER_SOFTVOL_SET, vol);
}

/**
 * @brief A decoder callbacks.
 *
 * This structure is filled in by the application and callbacks are called by the decoder.
 */
struct audio_decoder_callbacks {
	/**
	 * @brief Audio track info callback.
	 *
	 * This is called when track is loaded to set or clear the track info.
	 */
	void (*track_info)(const char *artist, const char *album, const char *title);
	/**
	 * @brief Audio track duration callback.
	 *
	 * @param duration_ms A duration of currently plaing song. If zero
	 *                    currently plaing song had finished.
	 */
	void (*track_duration)(long duration_ms);
	/**
	 * @brief Audio track art callback.
	 *
	 * This is called when track is loaded to set or clear the track cover art.
	 *
	 * @param buf A bufer with image that was embedded in the audio file.
	 * @param buf_len A buffer lenght in bytes.
	 */
	void (*track_art)(void *buf, size_t buf_len);
	/**
	 * @brief Updates current song offset from the start.
	 *
	 * @param offset_ms Current offset from the start of the song in miliseconds.
	 */
	void (*track_pos)(long offset_ms);
};

/**
 * @brief An audio decoder initialization.
 */
struct audio_decoder {
	/** @brief An audio decoder name. */
	const char *name;
	/**
	 * @brief An audio decoder init funciton.
	 *
	 * @param cbs An audio decoder app callbacks.
	 *
	 * @return An audio decoder ops.
	 */
	const struct audio_decoder_ops *(*init)(const struct audio_decoder_callbacks *cbs);
};

/**
 * @brief A NULL name terminated array of available decoders.
 */
extern const struct audio_decoder audio_decoders[];

/**
 * @brief Initialize audio decoder by name.
 *
 * The function falls back to first available decoder if decoder with a name
 * wasn't found.
 *
 * @param name An audio decoder name.
 * @param cbs An audio decoder app callbacks.
 */
const struct audio_decoder_ops *audio_decoder_init(const char *name,
                                                   const struct audio_decoder_callbacks *cbs);

/**
 * @brief A mpg123 init function.
 *
 * @param cbs An audio decoder app callbacks.
 *
 * @return An mpg123 decoder ops or NULL if mpg123 is not compiled in.
 */
const struct audio_decoder_ops *audio_decoder_mpg123(const struct audio_decoder_callbacks *cbs);

/**
 * @brief A mpv init function.
 *
 * @param cbs An audio decoder app callbacks.
 *
 * @return An mpv decoder ops or NULL if mpv is not compiled in.
 */
const struct audio_decoder_ops *audio_decoder_mpv(const struct audio_decoder_callbacks *cbs);

#endif /* AUDIO_DECODER_H */
