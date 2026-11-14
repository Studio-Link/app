/**
 * @file slua.h Studio Link User-Agent helpers (header only)
 *
 * Include after re.h and baresip.h
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef SLUA_H
#define SLUA_H

/**
 * Connect an outgoing call to a dial string like "500hz@studio.link" or
 * "stream"
 *
 * baresip >= 2.x does not complete the request URI in ua_connect() anymore
 * (baresip 1.x did), without "sip:" the call fails with ENOENT. The scheme
 * and the account domain are added if missing.
 *
 * @param ua    User-Agent
 * @param callp Optional pointer to allocated call object
 * @param dial  Dial string or SIP URI
 *
 * @return 0 if success, otherwise errorcode
 */
static inline int sl_ua_connect(struct ua *ua, struct call **callp,
				const char *dial)
{
	char *uri = NULL;
	struct pl pl;
	int err;

	if (!ua || !str_isset(dial))
		return EINVAL;

	pl_set_str(&pl, dial);

	err = account_uri_complete_strdup(ua_account(ua), &uri, &pl);
	if (err)
		return err;

	err = ua_connect(ua, callp, NULL, uri, VIDMODE_OFF);
	if (err)
		warning("sl_ua_connect: %s: %m\n", uri, err);

	mem_deref(uri);

	return err;
}

#endif /* SLUA_H */
