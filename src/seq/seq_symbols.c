/*
 *  Sequencer Symbols
 *  Copyright (c) 2001 by Jaroslav Kysela <perex@suse.cz>
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef PIC

extern const char *_snd_module_seq_hw;

static const char **snd_seq_open_objects[] = {
	&_snd_module_seq_hw
};
	
void *snd_seq_open_symbols(void)
{
	return (void *)snd_seq_open_objects[0];
}

#endif /* !PIC */
