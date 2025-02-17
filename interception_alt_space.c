#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// making r_alt+space to act as apostrophe (which is mapped to ISO_Level5_Shift)
// __u16 REDIRECT_TARGET = KEY_APOSTROPHE;
__u16 REDIRECT_TARGET;
// __u16 ALT_OR_META = KEY_RIGHTALT;
// __u16 ALT_OR_META = KEY_RIGHTMETA;
__u16 ALT_OR_META;
__u16 keyPressedWithHeldAlt;

FILE *logfile;

void logEvent(struct input_event event) {
  if (logfile != NULL) {
    fprintf(logfile, "type: %d, code: %d, value: %d\n", event.type, event.code,
            event.value);
  }
}

void logReceivedEvent(struct input_event event) {
  if (logfile != NULL) {
    fprintf(logfile, "RECEIVED: type: %d, code: %d, value: %d\n", event.type,
            event.code, event.value);
  }
}

void logMessage(char *restrict message) {
  if (logfile != NULL) {
    fprintf(logfile, message);
  }
}

void sendPressEvent(struct input_event event) {
  event.value = 1;
  fwrite(&event, sizeof(event), 1, stdout);
  logEvent(event);
}

void sendReleaseEvent(struct input_event event) {
  event.value = 0;
  fwrite(&event, sizeof(event), 1, stdout);
  logEvent(event);
}

void sendHoldEvent(struct input_event event) {
  event.value = 2;
  fwrite(&event, sizeof(event), 1, stdout);
  logEvent(event);
}

// This function sends the press and hold events for the target key
// when both ralt and space are held.
// In this case, it is apostrophe.
void sendTargetPressAndHoldEvents(int *both_are_held, struct input_event event,
                                  int alt_held_with_another_key) {

  if (alt_held_with_another_key) {
    event.code = ALT_OR_META;
    logMessage("releasing alt\n");
    sendReleaseEvent(event);
  }

  *both_are_held = 1;
  event.code = REDIRECT_TARGET;

  // press and hold the target key
  sendPressEvent(event);
  sendHoldEvent(event);

  // this key was observed but its pressed event was not sent yet
  if (alt_held_with_another_key) {
    event.code = keyPressedWithHeldAlt;
    logMessage("release key held with alt\n");
    sendPressEvent(event);
  }
}

// After both ralt and space are held, this function sends the hold event to the
// REDIRECT_TARGET
void sendTargetHoldEvent(struct input_event event) {
  event.code = REDIRECT_TARGET;
  fwrite(&event, sizeof(event), 1, stdout);
  logEvent(event);
}

// When only space or ralt is held, this function sends the press and the hold
// event for it.
void sendHoldEventForTheOnlyPressedKey(struct input_event event) {
  sendPressEvent(event);
  sendHoldEvent(event);
}

// When both ralt and space are released, this functions release both of them
// and the REDIRECT_TARGET
void releaseRedirectTarget(struct input_event event) {
  event.code = REDIRECT_TARGET;
  fwrite(&event, sizeof(event), 1, stdout);
  logEvent(event);
}

void sendReleaseEventForTheOnlyKey(struct input_event event) {
  event.value = 0;
  fwrite(&event, sizeof(event), 1, stdout);
  logEvent(event);
}

// When only ralt or space is released, this function sends the press and
// release event it
void sendPressAndReleaseEventForTheOnlyKey(struct input_event event) {
  sendPressEvent(event);
  sendReleaseEvent(event);
}

void printHelpAndExit() {
  fprintf(stderr, "Usage: PROGRAM \"KEY_RIGHTALT|KEY_RIGHTMETA\" KEYCODE "
                  "[logfilepath]\n\nYou can find the keycodes at "
                  "/usr/include/linux/input-event-codes.h");
  exit(1);
}

void grabArgs(int argc, char *argv[]) {
  if (argc == 1) {
    printHelpAndExit();
  }

  if (argc == 3) {
    REDIRECT_TARGET = atoi(argv[2]);
    if (strcmp(argv[1], "KEY_RIGHTALT") == 0) {
      ALT_OR_META = KEY_RIGHTALT;
    } else if (strcmp(argv[1], "KEY_RIGHTMETA") == 0) {
      ALT_OR_META = KEY_RIGHTMETA;
    } else {
      printHelpAndExit();
    }
  } else if (argc == 4) {
    REDIRECT_TARGET = atoi(argv[2]);
    if (strcmp(argv[1], "KEY_RIGHTALT") == 0) {
      ALT_OR_META = KEY_RIGHTALT;
    } else if (strcmp(argv[1], "KEY_RIGHTMETA") == 0) {
      ALT_OR_META = KEY_RIGHTMETA;
    } else {
      printHelpAndExit();
    }

    logfile = fopen(argv[3], "w");
  } else {
    printHelpAndExit();
  }
}

int main(int argc, char *argv[]) {

  grabArgs(argc, argv);

  setbuf(stdin, NULL), setbuf(stdout, NULL);

  struct input_event event;
  int grabbed_first_space = 0;
  int grabbed_first_alt = 0;
  int alt_held_with_another_key = 0;
  int space_pressed_with_another_key = 0;
  int both_are_held = 0;

  while (fread(&event, sizeof(event), 1, stdin) == 1) {

    logReceivedEvent(event);

    if (event.type == EV_KEY &&
        (event.code == KEY_SPACE || event.code == ALT_OR_META)) {

      if (!grabbed_first_space && event.code == KEY_SPACE && event.value == 1) {
        logMessage("grabbed space\n");
        grabbed_first_space = 1;

        if (grabbed_first_alt) {
          sendTargetPressAndHoldEvents(&both_are_held, event,
                                       alt_held_with_another_key);
        }
        continue;
      }

      if (!grabbed_first_alt && event.code == ALT_OR_META && event.value == 1) {
        logMessage("grabbed alt\n");
        grabbed_first_alt = 1;

        if (grabbed_first_space) {
          sendTargetPressAndHoldEvents(&both_are_held, event, 0);
        }
        continue;
      }

      // when space or ralt are held
      if (grabbed_first_space && grabbed_first_alt && event.value == 2) {
        if (alt_held_with_another_key) {
          event.code = keyPressedWithHeldAlt;
          sendHoldEvent(event);
        } else {
          sendTargetHoldEvent(event);
        }

        // when only one of them is held
      } else if (event.value == 2) {
        sendHoldEventForTheOnlyPressedKey(event);

        // treat release events
      } else if (event.value == 0) {
        // when one of them is released, release both
        if (both_are_held) {
          releaseRedirectTarget(event);

          // ignore release events one at a time
          if (event.code == KEY_SPACE) {
            grabbed_first_space = 0;
            if (space_pressed_with_another_key) {
              sendReleaseEventForTheOnlyKey(event);
              space_pressed_with_another_key = 0;
            }
          } else {
            if (alt_held_with_another_key) {
              sendReleaseEventForTheOnlyKey(event);
              alt_held_with_another_key = 0;
            }
            grabbed_first_alt = 0;
          }

          if (!grabbed_first_space && !grabbed_first_alt) {
            both_are_held = 0;
          }

          // release the one that was released
        } else {
          if (grabbed_first_space && event.code == KEY_SPACE) {
            if (space_pressed_with_another_key) {
              sendReleaseEventForTheOnlyKey(event);
              space_pressed_with_another_key = 0;
            } else {
              sendPressAndReleaseEventForTheOnlyKey(event);
            }
            grabbed_first_space = 0;
          } else if (grabbed_first_alt) {
            if (alt_held_with_another_key) {
              sendReleaseEventForTheOnlyKey(event);
              alt_held_with_another_key = 0;
            } else {
              sendPressAndReleaseEventForTheOnlyKey(event);
            }
            grabbed_first_alt = 0;
          }
        }
      }

      continue;
    }

    // handle the case when: ralt was pressed, but another key was pressed
    // before the hold was detected. This will apply the ralt as a normal key
    if (grabbed_first_alt && !grabbed_first_space && event.type == EV_KEY &&
        event.value == 1) {

      keyPressedWithHeldAlt = event.code;
      event.code = ALT_OR_META;

      sendPressEvent(event);
      sendHoldEvent(event);

      event.code = keyPressedWithHeldAlt;
      sendPressEvent(event);

      alt_held_with_another_key = 1;
      continue;
    }

    // handle the case when: space was pressed, but another key was pressed.
    // This will apply the space as a normal key
    if (!grabbed_first_alt && grabbed_first_space && event.type == EV_KEY) {
      if (event.value == 1) {
        __u16 keyToAppearAfterSpace = event.code;
        event.code = KEY_SPACE;
        sendPressEvent(event);

        event.code = keyToAppearAfterSpace;
        sendPressEvent(event);
        space_pressed_with_another_key = 1;
        continue;

        // handle the case when: key pressed -> space pressed -> key released
      } else if (event.value == 0 && !space_pressed_with_another_key) {
        __u16 keyToAppearAfterSpace = event.code;

        event.code = KEY_SPACE;
        sendPressEvent(event);
        event.code = keyToAppearAfterSpace;
        sendReleaseEvent(event);
        continue;
      }
    }

    fwrite(&event, sizeof(event), 1, stdout);
    logEvent(event);
    if (logfile != NULL) {
      fflush(logfile);
    }
  }
}
