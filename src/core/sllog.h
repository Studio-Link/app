/**
 * @file sllog.h Studio Link local log files
 *
 * Copyright (C) 2026 studio-link.de
 */

/*
 * Log files are written by the slogging module to <conf_path>/logs/.
 * Every start creates a new file, older files are rotated:
 *
 *   studio-link.0.log  current start
 *   studio-link.1.log  previous start
 *   ...
 *   studio-link.6.log  oldest kept start
 */

/** Log directory below the config path */
#define SL_LOG_DIR "logs"

/** Number of kept log files (starts) */
#define SL_LOG_KEEP 7

/** Log file name format, %u is the rotation index (0 = current) */
#define SL_LOG_FILE_FMT "studio-link.%u.log"
