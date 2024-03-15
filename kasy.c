#include "mappings.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>
#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>

static snd_seq_t *seq;
static snd_seq_addr_t *port;
static volatile sig_atomic_t stop = 0;

int mask = 0;

static void sighandler(int sig ATTRIBUTE_UNUSED) { stop = 1; }

Status send_event(Display *display, XEvent *key_event) {
  Status status =
      XSendEvent(display, InputFocus, False, KeyPressMask, key_event);
  if (status == 0) {
    fprintf(stderr, "Error: conversion to wire protocol format failed\n");
  } else if (status == BadValue) {
    fprintf(stderr, "Error: some numeric value falls outside the range of "
                    "values accepted by the request\n");
  } else if (status == BadWindow) {
    fprintf(stderr, "Error: a value for a Window argument does not name a "
                    "defined Window\n");
  }

  return status;
}

static void process_event(const snd_seq_event_t *ev, Display *display) {
  switch (ev->type) {
  case SND_SEQ_EVENT_NOTEON: {
    KeySym key = XStringToKeysym(mappings[ev->data.note.note]);
    KeyCode key_code = XKeysymToKeycode(display, key);
    if (key_code == 50) {
      mask |= ShiftMask;
    }
    XKeyPressedEvent key_event = {.type = KeyPress,
                                  .send_event = True,
                                  .display = display,
                                  /* .window = screen->root, */
                                  /* .root = screen->root, */
                                  .same_screen = True,
                                  .state = mask,
                                  .keycode = key_code};
    send_event(display, (XEvent *)&key_event);
    break;
  }
  case SND_SEQ_EVENT_NOTEOFF: {
    KeySym key = XStringToKeysym(mappings[ev->data.note.note]);
    KeyCode key_code = XKeysymToKeycode(display, key);
    if (key_code == 50) {
      mask = 0;
    }
    XKeyPressedEvent key_event = {.type = KeyRelease,
                                  .send_event = True,
                                  .display = display,
                                  /* .window = screen->root, */
                                  /* .root = screen->root, */
                                  .same_screen = True,
                                  .state = mask,
                                  .keycode = key_code};
    send_event(display, (XEvent *)&key_event);
    break;
  }
  }
}

const char *argp_program_version = "0.0.1";
static struct argp_option options[] = {{"port", 'p', "PORT", 0, "Port number (REQUIRED)", 0},
                                       {0, 'd', 0, 0, "Daemonize", 0},
                                       {0}};

static char doc[] = "Use your synthesizer as keyboard";
/* static char args_doc[] = "MAPPING"; */

struct arguments {
  char *port_name;
  bool daemon;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;
  int err;

  switch (key) {
  case 'p':
    arguments->port_name = arg;
    err = snd_seq_parse_address(seq, port, arguments->port_name);
    fprintf(stderr, "Parse port [%s]: %s\n", arg, snd_strerror(err));
    if (err < 0) {
      argp_usage(state);
    }
    break;
  case 'd':
    fprintf(stderr, "Daemonizing...\n");
    arguments->daemon = true;
    break;
  case ARGP_KEY_END:
    if (arguments->port_name == NULL) {
      argp_failure(state, 1, 0, "required -p option. See --help for more information");
      exit(ARGP_ERR_UNKNOWN);
    };
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, 0, doc, 0, 0, 0};

int main(int argc, char *argv[]) {
  struct arguments arguments = { 0 };
  if (argp_parse(&argp, argc, argv, 0, 0, &arguments) != 0) {
    perror("Arguments parsing error");
  };

  int err;
  err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
  fprintf(stderr, "Open sequencer: %s\n", snd_strerror(err));
  port = realloc(port, sizeof(snd_seq_addr_t));

  err = snd_seq_create_simple_port(
      seq, "aseqdump", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
      SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

  err = snd_seq_connect_from(seq, 0, port->client, port->port);
  fprintf(stderr, "Connect port [%d:%d]: %s\n", port->client, port->port,
          snd_strerror(err));
  if (err < 0)
    exit(EXIT_FAILURE);

  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Error: couldn't open X11 display\n");
  };

  Screen *screen = XDefaultScreenOfDisplay(display);
  if (screen == NULL) {
    fprintf(stderr, "Error: couldn't open X11 screen\n");
  };

  struct pollfd *pollfds;
  int npollfds;

  npollfds = snd_seq_poll_descriptors_count(seq, POLLIN);
  pollfds = alloca(sizeof(*pollfds) * npollfds);

  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);

  printf("Waiting for data:\n");

  for (;;) {
    snd_seq_poll_descriptors(seq, pollfds, npollfds, POLLIN);
    if (poll(pollfds, npollfds, -1) < 0)
      break;
    for (;;) {
      snd_seq_event_t *event;
      err = snd_seq_event_input(seq, &event);
      if (err < 0)
        break;
      if (event)
        process_event(event, display);
    }
    fflush(stdout);
    if (stop)
      break;
  }

  if (XCloseDisplay(display) != 0) {
    fprintf(stderr, "Error: couldn't close X11 display\n");
  };

  snd_seq_close(seq);
  return 0;
}
