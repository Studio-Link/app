/**
 * @file slopen.h Open an URL or folder with the default application (Windows)
 *
 * ShellExecuteW() is used instead of system("start ..."), which pops up a
 * cmd window (the standalone is a GUI subsystem app, the plugins run inside
 * a DAW) and breaks on paths with spaces.
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef SLOPEN_H
#define SLOPEN_H

#ifdef WIN32
#include <errno.h>
#include <windows.h>
#include <shellapi.h>


/**
 * Open target (URL or local path, UTF-8) with the default application
 *
 * @return 0 if success, otherwise errorcode
 */
static inline int sl_shell_open(const char *target)
{
	wchar_t wtarget[1024];
	HINSTANCE h;

	if (!target)
		return EINVAL;

	if (!MultiByteToWideChar(CP_UTF8, 0, target, -1, wtarget,
				 (int)(sizeof(wtarget) / sizeof(wtarget[0]))))
		return EINVAL;

	h = ShellExecuteW(NULL, L"open", wtarget, NULL, NULL, SW_SHOWNORMAL);

	return ((INT_PTR)h > 32) ? 0 : EIO;
}

#endif /* WIN32 */
#endif
