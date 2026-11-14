/**
 * @file slconf.h Studio Link configuration handling
 *
 * Copyright (C) 2026 studio-link.de
 */

/**
 * Configure baresip for Studio Link
 *
 * Uses ~/.studio-link (standalone), ~/.studio-link-plugin (SLPLUGIN) or
 * ~/.studio-link-onair (SLIVE) as config path. The config file is
 * (re)written from the Studio Link template if it does not exist, if it was
 * written by another Studio Link version or if reset is true.
 * A replaced config is kept as config.old.
 *
 * Replaces the old patched conf_configure(bool reset_conf) of baresip, call
 * it instead of conf_configure().
 *
 * @param reset Force a reset of the configuration
 *
 * @return 0 if success, otherwise errorcode
 */
int sl_conf_configure(bool reset);

/**
 * Use an explicit config path instead of the per-variant default
 * (must be called before sl_conf_configure)
 *
 * @param path Config path, NULL for default
 */
void sl_conf_path_set(const char *path);

/** User-Agent string used for SIP */
const char *sl_software(void);
