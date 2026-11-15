/**
 * @file winapp.c Windows desktop integration
 *
 * The Windows standalone is a GUI subsystem application (no console
 * window). This file provides:
 *
 *  - a notification area (tray) icon with a menu to open the web UI, the
 *    recordings and log folders and to quit Studio Link
 *  - single instance handling: starting Studio Link again opens the web UI
 *    of the running instance instead of starting a second one
 *  - error message boxes (there is no console to print to)
 *  - console output when started from a terminal (cmd/PowerShell)
 *
 * The tray window runs its own message loop in a separate thread, actions
 * are passed to the re main loop with a mqueue.
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <stdio.h>
#include <stdarg.h>
#include <re.h>
#include <baresip.h>
#include <windows.h>
#include <shellapi.h>
#include <slopen.h>
#include <sllog.h>
#include "winapp.h"
#include "resource.h"


#define SL_WIN_CLASS   L"StudioLinkStandaloneTray"
#define SL_WIN_MUTEX   L"Local\\StudioLinkStandalone"
#define SL_WIN_REGKEY  L"Software\\Studio Link"
#define SL_WIN_REGHINT L"TrayHintShown"

enum {
	WM_SL_TRAY     = WM_APP + 1,  /* notification icon callback   */
	WM_SL_GET_PORT = WM_APP + 2,  /* returns the web UI http port */
	SL_TRAY_UID    = 1,
};

enum {
	ID_OPEN = 100,
	ID_RECORDINGS,
	ID_LOGS,
	ID_QUIT,
};

static struct {
	HANDLE mutex;
	HANDLE thread;
	HANDLE ready;         /* tray window created (or failed)      */
	HANDLE stopped;       /* main loop finished, for WM_ENDSESSION */
	HWND hwnd;
	UINT msg_taskbar_created;
	volatile LONG port;
	struct mqueue *mq;
	sl_win_action_h *actionh;
	wchar_t tooltip[128];
	char logs[512];
	int err;
} win;


/* Console output if started from cmd/PowerShell (no-op from Explorer) */
void sl_win_console_attach(void)
{
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
		return;

	(void)freopen("CONOUT$", "w", stdout);
	(void)freopen("CONOUT$", "w", stderr);
}


static int utf8_to_wide(wchar_t *dst, size_t n, const char *src)
{
	return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int)n) ? 0
								    : EINVAL;
}


void sl_win_error(const char *fmt, ...)
{
	char msg[1024];
	wchar_t wmsg[1024];
	va_list ap;

	va_start(ap, fmt);
	(void)re_vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	warning("%s\n", msg);

	if (utf8_to_wide(wmsg, RE_ARRAY_SIZE(wmsg), msg))
		return;

	MessageBoxW(NULL, wmsg, L"Studio Link",
		    MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}


static void open_ui(uint16_t port)
{
	char url[64];

	if (!port)
		return;

	re_snprintf(url, sizeof(url), "http://127.0.0.1:%u/", port);
	(void)sl_shell_open(url);
}


/**
 * Single instance check: if Studio Link is already running, open its web
 * UI (this process has the foreground rights, the running one has not)
 *
 * @return true if another instance is running (and this one should exit)
 */
bool sl_win_activate_running(void)
{
	DWORD_PTR port = 0;
	HWND hwnd;
	int i;

	win.mutex = CreateMutexW(NULL, FALSE, SL_WIN_MUTEX);
	if (!win.mutex || GetLastError() != ERROR_ALREADY_EXISTS)
		return false;

	/* the running instance may still be starting up */
	for (i = 0; i < 50; i++) {
		hwnd = FindWindowW(SL_WIN_CLASS, NULL);
		if (hwnd && SendMessageTimeoutW(hwnd, WM_SL_GET_PORT, 0, 0,
						SMTO_ABORTIFHUNG, 1000, &port)
		    && port)
			break;

		Sleep(100);
	}

	open_ui((uint16_t)port);

	return true;
}


static bool hint_shown(void)
{
	DWORD val = 0, sz = sizeof(val);

	return RegGetValueW(HKEY_CURRENT_USER, SL_WIN_REGKEY, SL_WIN_REGHINT,
			    RRF_RT_REG_DWORD, NULL, &val, &sz) == ERROR_SUCCESS
	       && val;
}


static void hint_set_shown(void)
{
	DWORD val = 1;

	(void)RegSetKeyValueW(HKEY_CURRENT_USER, SL_WIN_REGKEY,
			      SL_WIN_REGHINT, REG_DWORD, &val, sizeof(val));
}


static void tray_icon_add(HWND hwnd, bool hint)
{
	NOTIFYICONDATAW nid = {0};
	HINSTANCE inst = GetModuleHandleW(NULL);

	nid.cbSize = sizeof(nid);
	nid.hWnd = hwnd;
	nid.uID = SL_TRAY_UID;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
	nid.uCallbackMessage = WM_SL_TRAY;
	nid.hIcon = LoadImageW(inst, MAKEINTRESOURCEW(IDI_STUDIOLINK),
			       IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
			       GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	wcsncpy(nid.szTip, win.tooltip, RE_ARRAY_SIZE(nid.szTip) - 1);

	if (hint) {
		/* first start: tell where Studio Link went */
		nid.uFlags |= NIF_INFO;
		nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON |
				  NIIF_RESPECT_QUIET_TIME;
		nid.hBalloonIcon = LoadImageW(inst,
				MAKEINTRESOURCEW(IDI_STUDIOLINK),
				IMAGE_ICON, GetSystemMetrics(SM_CXICON),
				GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
		wcsncpy(nid.szInfoTitle, L"Studio Link is running",
			RE_ARRAY_SIZE(nid.szInfoTitle) - 1);
		wcsncpy(nid.szInfo, L"Click the Studio Link icon to open it "
			L"again, right-click it to quit.",
			RE_ARRAY_SIZE(nid.szInfo) - 1);
	}

	(void)Shell_NotifyIconW(NIM_ADD, &nid);

	nid.uVersion = NOTIFYICON_VERSION_4;
	(void)Shell_NotifyIconW(NIM_SETVERSION, &nid);

	if (nid.hIcon)
		DestroyIcon(nid.hIcon);
	if (nid.hBalloonIcon)
		DestroyIcon(nid.hBalloonIcon);
}


static void tray_icon_remove(HWND hwnd)
{
	NOTIFYICONDATAW nid = {0};

	nid.cbSize = sizeof(nid);
	nid.hWnd = hwnd;
	nid.uID = SL_TRAY_UID;

	(void)Shell_NotifyIconW(NIM_DELETE, &nid);
}


static void tray_menu(HWND hwnd, int x, int y)
{
	HMENU menu = CreatePopupMenu();
	UINT flags = TPM_RIGHTBUTTON;
	int cmd;

	if (!menu)
		return;

	AppendMenuW(menu, MF_STRING | (win.port ? 0 : MF_GRAYED), ID_OPEN,
		    L"&Open Studio Link");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, ID_RECORDINGS,
		    L"Open &recordings folder");
	AppendMenuW(menu, MF_STRING, ID_LOGS, L"Open &log folder");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, ID_QUIT, L"&Quit Studio Link");
	SetMenuDefaultItem(menu, ID_OPEN, FALSE);

	flags |= GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN
							: TPM_LEFTALIGN;

	/* needed, otherwise the menu does not close on outside clicks */
	SetForegroundWindow(hwnd);
	cmd = TrackPopupMenuEx(menu, flags | TPM_RETURNCMD | TPM_NONOTIFY,
			       x, y, hwnd, NULL);
	PostMessageW(hwnd, WM_NULL, 0, 0);
	DestroyMenu(menu);

	switch (cmd) {

	case ID_OPEN:
		open_ui((uint16_t)win.port);
		break;

	case ID_RECORDINGS:
		(void)mqueue_push(win.mq, SL_WIN_OPEN_RECORDINGS, NULL);
		break;

	case ID_LOGS:
		(void)sl_shell_open(win.logs);
		break;

	case ID_QUIT:
		(void)mqueue_push(win.mq, SL_WIN_QUIT, NULL);
		break;

	default:
		break;
	}
}


static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (msg == win.msg_taskbar_created && msg) {
		/* Explorer was restarted */
		tray_icon_add(hwnd, false);
		return 0;
	}

	switch (msg) {

	case WM_SL_TRAY:
		switch (LOWORD(lp)) {

		case NIN_SELECT:
		case NIN_KEYSELECT:
			open_ui((uint16_t)win.port);
			break;

		case WM_CONTEXTMENU:
			tray_menu(hwnd, (short)LOWORD(wp), (short)HIWORD(wp));
			break;
		}
		return 0;

	case WM_SL_GET_PORT:
		return (LRESULT)win.port;

	case WM_ENDSESSION:
		if (wp) {
			/* logoff/shutdown: the process is terminated after
			 * returning, give the main loop time to clean up */
			(void)mqueue_push(win.mq, SL_WIN_QUIT, NULL);
			WaitForSingleObject(win.stopped, 5000);
		}
		return 0;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		tray_icon_remove(hwnd);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}


static DWORD WINAPI tray_thread(LPVOID arg)
{
	HINSTANCE inst = GetModuleHandleW(NULL);
	WNDCLASSEXW wc = {0};
	MSG msg;
	bool hint;
	(void)arg;

	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = wnd_proc;
	wc.hInstance = inst;
	wc.lpszClassName = SL_WIN_CLASS;
	wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_STUDIOLINK));

	if (!RegisterClassExW(&wc)) {
		win.err = EINVAL;
		SetEvent(win.ready);
		return 0;
	}

	win.msg_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

	/* hidden top-level window (message-only windows get neither
	 * WM_ENDSESSION nor TaskbarCreated and are not found by FindWindow) */
	win.hwnd = CreateWindowExW(0, SL_WIN_CLASS, L"Studio Link", 0,
				   0, 0, 0, 0, NULL, NULL, inst, NULL);
	if (!win.hwnd) {
		win.err = EINVAL;
		SetEvent(win.ready);
		return 0;
	}

	hint = !hint_shown();
	tray_icon_add(win.hwnd, hint);
	if (hint)
		hint_set_shown();

	SetEvent(win.ready);

	while (GetMessageW(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return 0;
}


static void mqueue_handler(int id, void *data, void *arg)
{
	(void)data;
	(void)arg;

	if (win.actionh)
		win.actionh((enum sl_win_action)id);
}


/**
 * Create the tray icon (call from the re main thread)
 *
 * @param actionh  Menu action handler, called on the re main thread
 * @param tooltip  Tray icon tooltip (UTF-8)
 *
 * @return 0 if success, otherwise errorcode
 */
int sl_win_tray_start(sl_win_action_h *actionh, const char *tooltip)
{
	char path[256];
	int err;

	win.actionh = actionh;
	(void)utf8_to_wide(win.tooltip, RE_ARRAY_SIZE(win.tooltip), tooltip);

	if (!conf_path_get(path, sizeof(path)))
		re_snprintf(win.logs, sizeof(win.logs), "%s\\" SL_LOG_DIR,
			    path);

	err = mqueue_alloc(&win.mq, mqueue_handler, NULL);
	if (err)
		return err;

	win.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
	win.stopped = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!win.ready || !win.stopped) {
		err = ENOMEM;
		goto out;
	}

	win.thread = CreateThread(NULL, 0, tray_thread, NULL, 0, NULL);
	if (!win.thread) {
		err = ENOMEM;
		goto out;
	}

	WaitForSingleObject(win.ready, INFINITE);
	err = win.err;

 out:
	if (err)
		sl_win_tray_stop();

	return err;
}


/** Set the web UI port (0 = not available yet) */
void sl_win_tray_set_port(uint16_t port)
{
	InterlockedExchange(&win.port, port);
}


/** Remove the tray icon, call at the end of the shutdown */
void sl_win_tray_stop(void)
{
	if (win.stopped)
		SetEvent(win.stopped);

	if (win.thread) {
		if (win.hwnd)
			PostMessageW(win.hwnd, WM_CLOSE, 0, 0);
		WaitForSingleObject(win.thread, 5000);
		CloseHandle(win.thread);
		win.thread = NULL;
	}

	win.hwnd = NULL;
	win.mq = mem_deref(win.mq);

	if (win.ready) {
		CloseHandle(win.ready);
		win.ready = NULL;
	}
	if (win.stopped) {
		CloseHandle(win.stopped);
		win.stopped = NULL;
	}
	if (win.mutex) {
		CloseHandle(win.mutex);
		win.mutex = NULL;
	}
}
