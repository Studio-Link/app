/**
 * @file macapp.h macOS app integration (Swift <-> Studio Link core)
 *
 * This header is also the Swift bridging header of the macOS app, so it
 * must not include re/baresip headers.
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef MACAPP_H
#define MACAPP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Swift side (any thread)
 */

/** Called on the core thread once Studio Link is ready (web UI port) */
typedef void (sl_mac_ready_h)(uint16_t port);

/**
 * Run the Studio Link core (blocking). Call from a dedicated thread, all
 * re/baresip code runs on that thread. Returns when the core has stopped.
 *
 * @return 0 if success, otherwise errorcode
 */
int sl_mac_core_main(int argc, char *argv[]);

/** Set the ready handler, call before sl_mac_core_main() */
void sl_mac_set_ready_handler(sl_mac_ready_h *readyh);

/** Stop the core (also if it is still starting up) */
void sl_mac_quit(void);

/** Open the folder of the current/latest recording in Finder */
void sl_mac_open_recordings(void);

/** Log folder (~/.studio-link/logs), valid after the ready handler */
const char *sl_mac_log_path(void);

/** Version string (e.g. "Studio Link v26.11.0 (arm64/darwin)") */
const char *sl_mac_software(void);

/** Error description (strerror) */
const char *sl_mac_strerror(int err);


/*
 * Core side (main.c, core thread)
 */

enum sl_mac_action {
	SL_MAC_QUIT,
	SL_MAC_OPEN_RECORDINGS,
};

typedef void (sl_mac_action_h)(enum sl_mac_action action);

int  sl_mac_start(sl_mac_action_h *actionh);
void sl_mac_ready(uint16_t port);
void sl_mac_stop(void);

#ifdef __cplusplus
}
#endif

#endif
