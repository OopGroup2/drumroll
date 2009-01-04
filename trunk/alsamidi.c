/*
   Copyright (C) 2009 Todd Kirby (ffmpeg.php@gmail.com)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "alsamidi.h"
#include <alsa/asoundlib.h>

struct OpaqueSeq {
    snd_seq_t * handle;
    int port;
};

struct OpaqueSeq* setup_sequencer(char *name, char* port_name)
{
    Seq seq = malloc(sizeof(struct OpaqueSeq));

	if (snd_seq_open(&seq->handle, "default", SND_SEQ_OPEN_OUTPUT, 0) != 0) {
		fprintf(stderr, "Unable to open sequencer handle\n");
        free(seq);
		return NULL;
	}

	snd_seq_set_client_name(seq->handle, name);
	seq->port = snd_seq_create_simple_port(seq->handle, port_name, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if(seq->port < 0) {
		fprintf(stderr, "Unable to create sequencer port\n");
        free(seq);
		return NULL;
	}

	return seq;
}


void send_event(unsigned int note, int velocity, bool pressed, struct OpaqueSeq *seq)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, seq->port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);

	snd_seq_ev_set_note(&ev, 0, note, velocity, 0);
    ev.type = SND_SEQ_EVENT_NOTEON;

	if (snd_seq_event_output(seq->handle, &ev) < 0) {
		fprintf(stderr, "Unable to send event\n");
	}

	snd_seq_drain_output(seq->handle);
}


void free_sequencer(struct OpaqueSeq* seq)
{
	if(seq->handle != NULL) {
		snd_seq_close(seq->handle);
	}

    if(seq->port > 0) {
		snd_seq_delete_simple_port(seq->handle, seq->port);
	}

    free(seq);
}

static void error_handler(const char *file, int line, const char *function, int err, const char *fmt, ...)
{
	va_list arg;

	if (err == ENOENT)	/* Ignore those misleading "warnings" */
		return;
	va_start(arg, fmt);
	fprintf(stderr, "ALSA lib %s:%i:(%s) ", file, line, function);
	vfprintf(stderr, fmt, arg);
	if (err)
		fprintf(stderr, ": %s", snd_strerror(err));
	putc('\n', stderr);
	va_end(arg);
}


int midiconnect(char* src, char* reciever)
{
	snd_seq_t *tmpseq;
	int queue = 0, convert_time = 0, convert_real = 0, exclusive = 0;
	int client;
	snd_seq_port_subscribe_t *subs;
	snd_seq_addr_t sender, dest;

	if (snd_seq_open(&tmpseq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr, "can't open sequencer\n");
		return 1;
	}
	
	snd_lib_error_set_handler(error_handler);

	if ((client = snd_seq_client_id(tmpseq)) < 0) {
		snd_seq_close(tmpseq);
		fprintf(stderr, "can't get client id\n");
		return 1;
	}

	/* set client info */
	if (snd_seq_set_client_name(tmpseq, "drumroll") < 0) {
		snd_seq_close(tmpseq);
		fprintf(stderr, "can't set client info\n");
		return 1;
	}

	/* set subscription */
	if (snd_seq_parse_address(tmpseq, &sender, src) < 0) {
		snd_seq_close(tmpseq);
		fprintf(stderr, "invalid sender address '%s' \n", src);
		return 1;
	}

	if (snd_seq_parse_address(tmpseq, &dest, reciever) < 0) {
		snd_seq_close(tmpseq);
		fprintf(stderr, "invalid destination address '%s'\n", reciever);
		return 1;
	}

	snd_seq_port_subscribe_alloca(&subs);
	snd_seq_port_subscribe_set_sender(subs, &sender);
	snd_seq_port_subscribe_set_dest(subs, &dest);
	snd_seq_port_subscribe_set_queue(subs, queue);
	snd_seq_port_subscribe_set_exclusive(subs, exclusive);
	snd_seq_port_subscribe_set_time_update(subs, convert_time);
	snd_seq_port_subscribe_set_time_real(subs, convert_real);

	snd_seq_close(tmpseq);

    return 0;
}
