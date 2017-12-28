/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "android_native_app_glue.h"
#include <android/log.h>

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h> // stdint formats
#include <stdlib.h> // rand
#include <time.h>   // clock_gettime
#include <wchar.h> // wmemset

#define  LOG_TAG    "libplasma"
#define  LOG(level, format, ...) \
	__android_log_print(level, LOG_TAG, "[%s:%d]\n" format, __func__, __LINE__, ##__VA_ARGS__)

typedef uint16_t color_16bits_t;
typedef int color_16bitsx2_t;
typedef uint8_t  color_8bits_channel_t;

#define make565(r,g,b) \
	((color_16bits_t) \
	 ((r >> 3) << 11) |\
	 ((g >> 2) << 5)  |\
	  (b >> 3) \
	)

typedef uint16_t window_pixel_t;

#define PIXEL_COLORS_MAX 4
#define PIXEL_COLORS_MAX_MASK 0b11
static inline uint_fast32_t pixel_colors_random_index()
{
	return (rand() & PIXEL_COLORS_MAX_MASK);
}

static void fill_pixels(ANativeWindow_Buffer* buffer)
{
	static color_16bits_t const pixel_colors[PIXEL_COLORS_MAX] = {
		make565(255,  0,  0),
		make565(  0,255,  0),
		make565(  0,  0,255),
		make565(255,255,  0)
	};

	/* Current pixel colors index */
	window_pixel_t * __restrict current_line_start = buffer->bits;

	size_t const window_line_width_weight =
		buffer->width * sizeof(window_pixel_t);
	uint_fast32_t const window_line_stride = buffer->stride;
	uint_fast32_t n_lines = buffer->height;

	while(n_lines--) {

		color_16bits_t const current_pixel_color =
			pixel_colors[pixel_colors_random_index()];
		color_16bitsx2_t const packed_colors =
			current_pixel_color << 16 | current_pixel_color;

		wmemset(
			(wchar_t *) current_line_start,
			packed_colors,
			window_line_width_weight);

		current_line_start += window_line_stride;
	}
}

struct engine {
	struct android_app* app;

	int animating;

	int32_t initial_window_format;

	struct {
		uint_fast32_t i;
		uint_fast32_t data[8];
	} times;
};

#ifndef __cplusplus
	enum bool { false, true };
	typedef enum bool bool;
#endif

static inline bool engine_have_a_window
(struct engine const * __restrict const engine)
{
	return engine->app->window != NULL;
}

static void engine_times_store
(struct engine * __restrict const engine,
 uint_fast32_t              const time_spent)
{
	engine->times.data[engine->times.i++] = time_spent;
	engine->times.i &= 7;
}

static inline bool engine_times_wrapped_around
(struct engine const * __restrict const engine)
{
	return engine->times.i == 0;
}

static uint_fast32_t engine_times_average
(struct engine const * __restrict const engine)
{
	uint_fast32_t const * __restrict const times =
		engine->times.data;

	uint_fast64_t const times_sum =
		times[0] + times[1] + times[2] + times[3] +
		times[4] + times[5] + times[6] + times[7];

	uint_fast32_t const times_average =
		times_sum / 8;

	return times_average;
}

static void engine_draw_frame(struct engine* engine)
{

	ANativeWindow_Buffer buffer;

	if (!engine_have_a_window(engine))
	{
		LOG(ANDROID_LOG_WARN, "The engine doesn't have a window !?\n");
		goto draw_frame_end;
	}

	if (ANativeWindow_lock(engine->app->window, &buffer, NULL) < 0)
	{
		LOG(ANDROID_LOG_WARN, "Could not lock the window... :C\n");
		goto draw_frame_end;
	}

	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	uint_fast32_t const before_ns = time.tv_nsec;

	fill_pixels(&buffer);
	ANativeWindow_unlockAndPost(engine->app->window);

	clock_gettime(CLOCK_MONOTONIC, &time);
	uint_fast32_t const after_ns = time.tv_nsec;

	uint_fast32_t time_spent;

	if (after_ns > before_ns)
		time_spent = after_ns - before_ns;
	else
		time_spent = (1000000000L - before_ns) + after_ns;

	engine_times_store(engine, time_spent);
	if (engine_times_wrapped_around(engine))
		LOG(ANDROID_LOG_INFO, "Average : %" PRIuFAST32 "\n",
			engine_times_average(engine));

draw_frame_end:
	return;
}

static inline void engine_term_display
(struct engine * __restrict const engine)
{
	engine->animating = 0;
}

static int32_t engine_handle_input
(struct android_app * app, AInputEvent * event) {
	struct engine * const engine =
		(struct engine *) app->userData;

	int32_t const current_event_type =
		AInputEvent_getType(event);

	if (current_event_type == AINPUT_EVENT_TYPE_MOTION)
	{
		engine->animating = 1;
		return 1;
	}
	else if (current_event_type == AINPUT_EVENT_TYPE_KEY)
	{
		LOG(ANDROID_LOG_INFO,
			"Key event: action=%d keyCode=%d metaState=0x%x",
			AKeyEvent_getAction(event),
			AKeyEvent_getKeyCode(event),
			AKeyEvent_getMetaState(event));
	}

	return 0;
}

static void engine_handle_cmd
(struct android_app* app, int32_t cmd)
{
	struct engine * __restrict const engine =
		(struct engine *) app->userData;

	switch (cmd) {
		case APP_CMD_INIT_WINDOW:
			if (engine_have_a_window(engine))
			{

				engine->initial_window_format =
					ANativeWindow_getFormat(app->window);

				ANativeWindow_setBuffersGeometry(app->window,
					ANativeWindow_getWidth(app->window),
					ANativeWindow_getHeight(app->window),
					WINDOW_FORMAT_RGB_565);

				engine_draw_frame(engine);
			}
			break;
		case APP_CMD_TERM_WINDOW:
			engine_term_display(engine);

			ANativeWindow_setBuffersGeometry(app->window,
				ANativeWindow_getWidth(app->window),
				ANativeWindow_getHeight(app->window),
				engine->initial_window_format);

			break;
		case APP_CMD_LOST_FOCUS:
			engine->animating = 0;
			engine_draw_frame(engine);
			break;
	}
}

void android_main(struct android_app* state) {

	struct engine engine = {0};

	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;

	// loop waiting for stuff to do.

	while (1) {
		// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		// If not animating, we will block forever waiting for events.
		// If animating, we loop until all events are read, then continue
		// to draw the next frame of animation.
		while (
			(ident=ALooper_pollAll(
				engine.animating ? 0 : -1,
				NULL, &events, (void **) &source))
			>= 0)
		{

			// Process this event.
			if (source != NULL) {
				source->process(state, source);
			}

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				LOG(ANDROID_LOG_INFO,
					"Engine thread destroy requested!");
				engine_term_display(&engine);
				return;
			}
		}

		if (engine.animating) {
			engine_draw_frame(&engine);
		}
	}
}

