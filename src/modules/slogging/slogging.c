/**
 * @file slogging.c Local file logging
 *
 * Writes all baresip log messages to <conf_path>/logs/studio-link.0.log.
 * On every start the previous files are rotated and the last SL_LOG_KEEP
 * starts are kept (see sllog.h).
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <re.h>
#include <baresip.h>
#include <sllog.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef WIN32
#include <share.h>
#endif


static FILE *log_fp = NULL;
static mtx_t *lock = NULL;


static const char *level_name(uint32_t level)
{
	static const char *names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

	return names[MIN(level, RE_ARRAY_SIZE(names) - 1)];
}


/* May be called from any thread */
static void log_handler(uint32_t level, const char *msg)
{
	char ts[32] = "";
	time_t now = time(NULL);
	struct tm tm;
	size_t len;

	if (!msg)
		return;

#ifdef WIN32
	if (!localtime_s(&tm, &now))
#else
	if (localtime_r(&now, &tm))
#endif
		strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

	len = strlen(msg);

	mtx_lock(lock);
	if (log_fp) {
		(void)fprintf(log_fp, "%s %-5s %s%s", ts, level_name(level),
			      msg,
			      (len && msg[len - 1] == '\n') ? "" : "\n");
		(void)fflush(log_fp);
	}
	mtx_unlock(lock);
}


static struct log lg = {
	.h = log_handler,
};


static int log_file_path(char *buf, size_t sz, const char *dir,
			 unsigned idx)
{
	if (re_snprintf(buf, sz, "%s/" SL_LOG_FILE_FMT, dir, idx) < 0)
		return ENOMEM;

	return 0;
}


/* Shift studio-link.N.log -> studio-link.N+1.log, drop the oldest */
static void log_rotate(const char *dir)
{
	char from[512], to[512];
	unsigned i;

	if (log_file_path(to, sizeof(to), dir, SL_LOG_KEEP - 1))
		return;

	(void)remove(to);

	for (i = SL_LOG_KEEP - 1; i > 0; i--) {
		if (log_file_path(from, sizeof(from), dir, i - 1) ||
		    log_file_path(to, sizeof(to), dir, i))
			return;

		/* Windows rename() fails if the target exists */
		(void)remove(to);
		(void)rename(from, to);
	}
}


static int module_init(void)
{
	char dir[256];
	char file[512];
	int err;

	err = conf_path_get(dir, sizeof(dir));
	if (err)
		return err;

	if (re_snprintf(dir + strlen(dir), sizeof(dir) - strlen(dir),
			"/" SL_LOG_DIR) < 0)
		return ENOMEM;

	if (!fs_isdir(dir)) {
		err = fs_mkdir(dir, 0700);
		if (err) {
			warning("slogging: mkdir %s failed (%m)\n", dir, err);
			return err;
		}
	}

	log_rotate(dir);

	err = log_file_path(file, sizeof(file), dir, 0);
	if (err)
		return err;

	err = mutex_alloc(&lock);
	if (err)
		return err;

#ifdef WIN32
	/* fs_fopen() uses fopen_s(), which opens without sharing: the
	 * webapp log download (and other readers) could not read the
	 * current log while it is open. Allow readers, deny writers. */
	log_fp = _fsopen(file, "w", _SH_DENYWR);
	err = log_fp ? 0 : errno;
#else
	err = fs_fopen(&log_fp, file, "w");
#endif
	if (err) {
		warning("slogging: open %s failed (%m)\n", file, err);
		lock = mem_deref(lock);
		return err;
	}

	log_register_handler(&lg);

	info("slogging: started (%s)\n", dir);
	info("slogging: Version %s (baresip %s)\n", SLVERSION,
	     BARESIP_VERSION);
	info("slogging: Machine %s/%s\n", sys_arch_get(), sys_os_get());
	info("slogging: Kernel %H\n", sys_kernel_get, NULL);
	info("slogging: Build %H\n", sys_build_get, NULL);
	info("slogging: Network %H\n", net_debug, baresip_network());

	return 0;
}


static int module_close(void)
{
	log_unregister_handler(&lg);

	mtx_lock(lock);
	if (log_fp) {
		(void)fclose(log_fp);
		log_fp = NULL;
	}
	mtx_unlock(lock);

	lock = mem_deref(lock);

	return 0;
}


const struct mod_export DECL_EXPORTS(slogging) = {
	"slogging",
	"application",
	module_init,
	module_close
};
