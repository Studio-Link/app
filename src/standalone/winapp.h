/**
 * @file winapp.h Windows desktop integration (tray icon, single instance)
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef WINAPP_H
#define WINAPP_H

#ifdef WIN32

/** Tray menu actions, delivered on the main (re) thread */
enum sl_win_action {
	SL_WIN_QUIT,
	SL_WIN_OPEN_RECORDINGS,
};

typedef void (sl_win_action_h)(enum sl_win_action action);

void sl_win_console_attach(void);
bool sl_win_activate_running(void);
int  sl_win_tray_start(sl_win_action_h *actionh, const char *tooltip);
void sl_win_tray_set_port(uint16_t port);
void sl_win_tray_stop(void);
void sl_win_error(const char *fmt, ...);

#endif
#endif
