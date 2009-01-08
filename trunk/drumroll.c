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

//#include <config.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <usb.h>

#include "usb_utils.h"

#ifdef HAVE_LIBASOUND
#include "alsamidi.h"
#endif

#ifdef HAVE_LIBJACK
#include "jackmidi.h"
#endif

#ifdef HAVE_LIBSDL_MIXER
#include "sdlaudio.h"
#endif

#define USB_VENDOR_ID_DREAM_CHEEKY 0x1941
#define USB_DEVICE_ID_ROLL_UP_DRUMKIT 0x8021
#define USB_INTERFACE_NUMBER 0x00
#define USB_ENDPOINT 0x81

#define NUM_PADS 6

static volatile sig_atomic_t fatal_error_in_progress = 0;

// GLOBAL COMMANDLINE FLAGS
static int nosound = false;
static int alsamidi = false;
static int autoconnect_hydrogen = false;
static int jackmidi = false;
static int verbose = false;

static usb_dev_handle* drumkit_handle = NULL;

/* A single drum pad */
typedef struct {
#ifdef HAVE_LIBSDL_MIXER
    Sound sound;
#endif
} Pad;

/*
 * Drumkit event loop
 */
static int start_processing_drum_events(usb_dev_handle* drumkit_handle, Pad *pads)
{
    char drum_state, last_drum_state = 0;
    int pad_num;

    // read pad status from device
    while (usb_bulk_read(drumkit_handle, USB_ENDPOINT, &drum_state, 1, 0) >= 0) {

        if (drum_state == last_drum_state) {
            continue;
        }

        for (pad_num = 0; pad_num < NUM_PADS; pad_num++) {
            if (((drum_state ^ last_drum_state) & drum_state) & (1 << pad_num)) {
#ifdef HAVE_LIBSDL_MIXER
                if (!nosound) {
                    play_sound(pads[pad_num].sound);
                }
#endif

#ifdef HAVE_LIBASOUND
                if (alsamidi) {
                    send_event(36 + pad_num, 127, true);
                }
#endif
            }
        }
        last_drum_state = drum_state;

#ifdef HAVE_LIBJACK
        if (jackmidi) {
            set_jack_state(drum_state);
        }
#endif

    }

    fprintf(stderr, "ERROR: reading from usb device.\n Reason: %s\n", strerror(errno));
    return -1;
}

#ifdef HAVE_LIBSDL_MIXER
static int load_sounds(Pad *pads) {
    char filename[1024];

    int pad_num;

    for (pad_num = 0; pad_num < NUM_PADS; pad_num++) {
        sprintf(filename, "%s/pad%d.wav", SAMPLESDIR, pad_num + 1);

        if (verbose) {
            fprintf(stdout, "Loading sound file '%s' into pad %d\n", filename, pad_num + 1);
        }

        if (access(filename, R_OK) < 0) {
            fprintf(stderr, "ERROR: Can't load sound file '%s'.\n Reason: %s\n", 
                    filename, strerror(errno));
            return 1;
        }

        pads[pad_num].sound = load_sound(filename, pad_num);

        if (pads[pad_num].sound == NULL) {
            fprintf(stderr,"Could not load %s\n", filename);
            return 2;
        }
    }

    return 0;
}
#endif

static void print_usage(char * program_name)
{
    fprintf(stdout, "Usage: %s [OPTIONS]\n\n", program_name);
#ifdef HAVE_LIBASOUND
    fprintf(stdout, "  -a, --alsamidi\n");
    //fprintf(stdout, "  -A, --autoconnect-hydrogen\n");
#endif
#ifdef HAVE_LIBJACK
    fprintf(stdout, "  -j, --jackmidi\n");
#endif
#ifdef HAVE_LIBSDL_MIXER
    fprintf(stdout, "  -n, --nosound\n");
#endif
    fprintf(stdout, "  -v, --verbose\n");
    fprintf(stdout, "  -V, --version\n");
}

static void parse_options(int argc, char** argv)
{
    int opt_index = 0;
    char c;

    static const struct option long_options[] = {
        {"help", no_argument,       0, 'h'},
#ifdef HAVE_LIBASOUND
        {"alsamidi", no_argument,   0, 'a'},
        //{"autoconnect-hydrogen", no_argument,   0, 'A'},
#endif
#ifdef HAVE_LIBJACK
        {"jackmidi", no_argument,   0, 'j'},
#endif
#ifdef HAVE_LIBSDL_MIXER
        {"nosound", no_argument,    0, 'n'},
#endif
        {"verbose", no_argument,    0, 'v'},
        {"version", no_argument,    0, 'V'},
        {0, 0, 0, 0}
    };


    while ((c = getopt_long(argc, argv, "AhjanvV", long_options, &opt_index)) != -1) {

        if (c == 0) {
            c = long_options[opt_index].val;
        }

        switch(c) {
#ifdef HAVE_LIBASOUND
            case 'a':
                alsamidi = true;
                break;
            case 'A':
                autoconnect_hydrogen = true;
                break;
#endif
#ifdef HAVE_LIBJACK
            case 'j':
                jackmidi = true;
                break;
#endif
#ifdef HAVE_LIBSDL_MIXER
            case 'n':
                nosound = true;
                printf("Sound Disabled\n");
                break;
#endif
            case 'h':
                print_usage(argv[0]);
                exit(0);
                break;
            case 'v':
                verbose = true;
                break;
            case 'V':
                fprintf(stdout, "%s\n", PACKAGE_STRING);
                exit(0);
                break;
            default:
                fprintf(stdout, "unrecognized option %c\n", c);
                exit(0);
                break;
        }
    }
}


static void cleanup()
{
    usb_release_and_close_device(drumkit_handle, USB_INTERFACE_NUMBER);

#ifdef HAVE_LIBSDL_MIXER
    if (!nosound) {
        close_audio();
    }
#endif

#ifdef HAVE_LIBASOUND
    if (alsamidi) {
        free_sequencer();
    }
#endif

    exit(0);
}


void termination_handler(int sig)
{
    /* Since this handler is established for more than one kind of signal, 
       it might still get invoked recursively by delivery of some other kind
       of signal.  Use a static variable to keep track of that. */
    if (fatal_error_in_progress) {
        raise (sig);
    }
    fatal_error_in_progress = 1;

    cleanup();

    /* Now reraise the signal.  We reactivate the signal's
       default handling, which is to terminate the process.
       We could just call exit or abort,
       but reraising the signal sets the return status
       from the process correctly. */
    signal (sig, SIG_DFL);
    raise (sig);
}


int main(int argc, char** argv)
{
    struct usb_device* usb_drumkit_device = NULL;  
    Pad pads[NUM_PADS];

    // Set signals so we can do orderly cleanup when user
    // terminates us.
    if (signal (SIGINT, termination_handler) == SIG_IGN)
        signal (SIGINT, SIG_IGN);
    if (signal (SIGHUP, termination_handler) == SIG_IGN)
        signal (SIGHUP, SIG_IGN);
    if (signal (SIGTERM, termination_handler) == SIG_IGN)
        signal (SIGTERM, SIG_IGN);


    parse_options(argc, argv);

    usb_drumkit_device = get_usb_device(USB_VENDOR_ID_DREAM_CHEEKY, USB_DEVICE_ID_ROLL_UP_DRUMKIT);

    if (usb_drumkit_device  == NULL) {
        fprintf(stderr, "ERROR: couldn't find drumkit. Quitting...\n");
        exit(1);
    }

    drumkit_handle = usb_open(usb_drumkit_device);

    if (drumkit_handle == NULL) {
        fprintf(stderr, "ERROR: opening drumkit. Quitting\n");
        exit(2);
    }

    if (claim_usb_device(drumkit_handle, 0x00)) {
        fprintf(stderr, "ERROR: claiming drumkit. Quitting\n");
        exit(3);
    }

#ifdef HAVE_LIBSDL_MIXER
    if (!nosound) { 
        if (init_audio() != 0) {
            fprintf(stderr, "ERROR: audio initialization failed. Quitting\n");
            exit(4);
        }

        if (load_sounds(pads)) {
            fprintf(stderr, "ERROR: loading sounds. Quitting.\n");
            exit(5);
        }
    }
#endif

#ifdef HAVE_LIBASOUND
    if (alsamidi) {
        if (setup_sequencer(PACKAGE_NAME, "Output")) {
            exit(5);
        }

        if (autoconnect_hydrogen) {
            //midiconnect("drumroll", "Hydrogen");
        }
    }
#endif

#ifdef HAVE_LIBJACK
    if (jackmidi) {
        if (jack_init(NUM_PADS, 0)) {
            fprintf(stderr, "ERROR: jack initialization failed\n");
            exit(6);
        }
    }
#endif

    start_processing_drum_events(drumkit_handle, pads);

    cleanup(drumkit_handle);

    return 0;
}
