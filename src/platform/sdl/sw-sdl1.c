/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>

static bool mSDLSWInit(struct mSDLRenderer* renderer);
static void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLSWDeinit(struct mSDLRenderer* renderer);

void mSDLSWCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLSWInit;
	renderer->deinit = mSDLSWDeinit;
	renderer->runloop = mSDLSWRunloop;
}

bool mSDLSWInit(struct mSDLRenderer* renderer) {
#ifdef COLOR_16_BIT
	renderer->screen = SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE | (SDL_FULLSCREEN * renderer->player.fullscreen));
#else
	renderer->screen = SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE | (SDL_FULLSCREEN * renderer->player.fullscreen));
#endif
	SDL_WM_SetCaption(projectName, "");

	unsigned width, height;
	renderer->core->desiredVideoDimensions(renderer->core, &width, &height);
	SDL_LockSurface(renderer->screen);

	bool formatMatch = (renderer->screen->format->BytesPerPixel == BYTES_PER_PIXEL &&
	                    renderer->screen->format->Rmask == M_COLOR_RED && renderer->screen->format->Gmask == M_COLOR_GREEN &&
	                    renderer->screen->format->Bmask == M_COLOR_BLUE && renderer->screen->format->Amask == M_COLOR_ALPHA);

	if (renderer->ratio == 1 && formatMatch) {
		renderer->core->setVideoBuffer(renderer->core, renderer->screen->pixels, renderer->screen->pitch / BYTES_PER_PIXEL);
	} else {
		renderer->surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, BYTES_PER_PIXEL * 8, M_COLOR_RED, M_COLOR_GREEN, M_COLOR_BLUE, 0);
		renderer->core->setVideoBuffer(renderer->core, renderer->surface->pixels, renderer->surface->pitch / BYTES_PER_PIXEL);

		if (renderer->ratio > 1 && !formatMatch)
			renderer->tempSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, renderer->screen->format->BitsPerPixel, renderer->screen->format->Rmask, renderer->screen->format->Gmask, renderer->screen->format->Bmask, renderer->screen->format->Amask);
	}

	return true;
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;

	while (mCoreThreadIsActive(context)) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			SDL_UnlockSurface(renderer->screen);
			if (renderer->ratio > 1) {
				if (renderer->tempSurface) {
					if (SDL_BlitSurface(renderer->surface, NULL, renderer->tempSurface, NULL) < 0) {
						fprintf(stderr, "%s", SDL_GetError());
						fflush(stderr);
						abort();
					}
					SDL_SoftStretch(renderer->tempSurface, NULL, renderer->screen, NULL);
				} else {
					SDL_SoftStretch(renderer->surface, NULL, renderer->screen, NULL);
				}
			} else if (renderer->surface) {
				SDL_BlitSurface(renderer->surface, NULL, renderer->screen, NULL);
			}
			SDL_Flip(renderer->screen);
			SDL_LockSurface(renderer->screen);
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
	}
}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->tempSurface) {
		SDL_FreeSurface(renderer->tempSurface);
	}
	if (renderer->surface) {
		SDL_FreeSurface(renderer->surface);
	}
	SDL_UnlockSurface(renderer->screen);
}
