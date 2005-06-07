/**
 * \file mixer/simple_abst.c
 * \brief Mixer Simple Element Class Interface - Module Abstraction
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2005
 *
 * Mixer simple element class interface.
 */
/*
 *  Mixer Interface - simple controls - abstraction module
 *  Copyright (c) 2005 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include <dlfcn.h>
#include "config.h"
#include "asoundlib.h"
#include "mixer_simple.h"

#define SO_PATH PKGLIBDIR "/smixer/"

typedef struct _class_priv {
	char *device;
	snd_ctl_t *ctl;
	snd_hctl_t *hctl;
	snd_ctl_card_info_t *info;
} class_priv_t;   

static int try_open(snd_mixer_class_t *class, const char *lib)
{
	snd_mixer_event_t event_func;
	char *xlib;
	void *h;

	xlib = malloc(strlen(lib) + strlen(SO_PATH) + 1);
	if (xlib == NULL)
		return -ENOMEM;
	strcpy(xlib, SO_PATH);
	strcat(xlib, lib);
	h = snd_dlopen(lib, RTLD_NOW);
	if (h == NULL) {
		SNDERR("Unable to open library '%s'", xlib);
		free(xlib);
		return -ENXIO;
	}
	event_func = dlsym(h, "alsa_mixer_simple_event");
	if (event_func == NULL) {
		SNDERR("Symbol 'alsa_mixer_simple_event' was not found in '%s'", xlib);
		snd_dlclose(h);
		free(xlib);
		return -ENXIO;
	}
	free(xlib);
	snd_mixer_class_set_event(class, event_func);
	return 0;
}

static int match(snd_mixer_class_t *class, const char *lib, const char *searchl)
{
	class_priv_t *priv = snd_mixer_class_get_private(class);
	const char *components;

	if (searchl == NULL)
		return try_open(class, lib);
	components = snd_ctl_card_info_get_components(priv->info);
	while (*components != '\0') {
		if (!strncmp(components, searchl, strlen(searchl)))
			return try_open(class, lib);
		while (*components != ' ' && *components != '\0')
			components++;
		while (*components == ' ' && *components != '\0')
			components++;
	}
	return 0;
}

static int find_module(snd_mixer_class_t *class, snd_config_t *top)
{
	snd_config_iterator_t i, next;
	snd_config_iterator_t j, jnext;
	char *lib, *searchl;
	const char *id;
	int err;

	snd_config_for_each(i, next, top) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;
		searchl = NULL;
		lib = NULL;
		snd_config_for_each(j, jnext, n) {
			snd_config_t *m = snd_config_iterator_entry(j);
			if (snd_config_get_id(m, &id) < 0)
				continue;
			if (!strcmp(id, "searchl")) {
				err = snd_config_get_string(m, (const char **)&searchl);
				if (err < 0)
					return err;
				continue;
			}
			if (!strcmp(id, "lib")) {
				err = snd_config_get_string(m, (const char **)&lib);
				if (err < 0)
					return err;
				continue;
			}
		}
		err = match(class, lib, searchl);
		if (err == 1)
			return 0;
		if (err < 0)
			return err;
	}
	return -ENOENT;
}

static void private_free(snd_mixer_class_t *class)
{
	class_priv_t *priv = snd_mixer_class_get_private(class);
	
	if (priv->info)
		snd_ctl_card_info_free(priv->info);
	if (priv->hctl)
		snd_hctl_close(priv->hctl);
	else if (priv->ctl)
		snd_ctl_close(priv->ctl);
	if (priv->device)
		free(priv->device);
	free(priv);
}

/**
 * \brief Register mixer simple element class - basic abstraction
 * \param mixer Mixer handle
 * \param options Options container
 * \param classp Pointer to returned mixer simple element class handle (or NULL
 * \return 0 on success otherwise a negative error code
 */
int snd_mixer_simple_basic_register(snd_mixer_t *mixer,
				    struct snd_mixer_selem_regopt *options,
				    snd_mixer_class_t **classp)
{
	snd_mixer_class_t *class;
	class_priv_t *priv = calloc(1, sizeof(*priv));
	const char *file;
	snd_input_t *input;
	snd_config_t *top = NULL;
	int err;

	if (options->device == NULL)
		return -EIO;
	if (priv == NULL)
		return -ENOMEM;
	if (snd_mixer_class_malloc(&class)) {
		free(priv);
		return -ENOMEM;
	}
	priv->device = strdup(options->device);
	if (priv->device == NULL) {
		free(priv);
		snd_mixer_class_free(class);
		return -ENOMEM;
	}
	snd_mixer_class_set_compare(class, snd_mixer_selem_compare);
	snd_mixer_class_set_private(class, priv);
	snd_mixer_class_set_private_free(class, private_free);
	err = snd_ctl_open(&priv->ctl, priv->device, 0);
	if (err < 0) {
		SNDERR("unable to open control device '%s': %s", priv->device, snd_strerror(err));
		goto __error;
	}
	err = snd_hctl_open_ctl(&priv->hctl, priv->ctl);
	if (err < 0)
		goto __error;
	err = snd_ctl_card_info_malloc(&priv->info);
	if (err < 0)
		goto __error;
	err = snd_ctl_card_info(priv->ctl, priv->info);
	if (err < 0)
		goto __error;
	file = getenv("ALSA_MIXER_SIMPLE");
	if (!file)
		file = DATADIR "/alsa/smixer.conf";
	err = snd_config_top(&top);
	if (err >= 0) {
		err = snd_input_stdio_open(&input, file, "r");
		if (err < 0) {
			SNDERR("unable to open simple mixer configuration file '%s'", file);
			goto __error;
		}
		err = snd_config_load(top, input);
		snd_input_close(input);
		if (err < 0) {
			SNDERR("%s may be old or corrupted: consider to remove or fix it", file);
			goto __error;
		}
		err = find_module(class, top);
	}
	if (err >= 0)
		err = snd_mixer_class_register(class, mixer);
	if (err < 0) {
	      __error:
	      	if (top)
	      		snd_config_delete(top);
	      	if (class)
			snd_mixer_class_free(class);
		return err;
	}
	if (top)
		snd_config_delete(top);
	if (classp)
		*classp = class;
	return 0;
}