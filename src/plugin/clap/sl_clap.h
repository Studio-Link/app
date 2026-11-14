/**
 * @file sl_clap.h Studio Link CLAP entry functions
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef SL_CLAP_H
#define SL_CLAP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sl_clap_entry_init(const char *plugin_path);
void sl_clap_entry_deinit(void);
const void *sl_clap_entry_get_factory(const char *factory_id);

#ifdef __cplusplus
}
#endif

#endif /* SL_CLAP_H */
