#define SLVERSION "SLVERSION_T"
#ifndef SLPLUGIN 
#include <pthread.h>
#include "FLAC/stream_encoder.h"
#endif

#ifdef SLPLUGIN
/*
 * effect/effect.c
 */
void effect_set_bypass(bool value);
#endif

enum ws_type {
	WS_BARESIP,
	WS_CALLS,
	WS_CONTACT,
	WS_CHAT,
	WS_METER,
	WS_CPU,
	WS_OPTIONS,
	WS_RTAUDIO
};

enum webapp_call_state {
	WS_CALL_OFF,
	WS_CALL_RINGING,
	WS_CALL_ON
};

enum {
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

struct webapp {
	struct websock_conn *wc_srv;
	struct le le;
	enum ws_type ws_type;
};

enum webapp_call_state webapp_call_status;
struct odict *webapp_calls;


/*
 * sessions.c
 */
void webapp_sessions_init(void);
void webapp_sessions_close(void);
int webapp_session_delete(char * const sess_id, struct call *call);
int8_t webapp_call_update(struct call *call, char *state);
int webapp_session_stop_stream(void);
struct call* webapp_session_get_call(char * const sess_id);
bool webapp_session_available(void);


/*
 * vumeter.c
 */
int webapp_vu_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm,
			 const struct audio *au);
int webapp_vu_decode_update(struct aufilt_dec_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm,
			 const struct audio *au);
int webapp_vu_encode(struct aufilt_enc_st *st, void *sampv, size_t *sampc);
int webapp_vu_decode(struct aufilt_dec_st *st, void *sampv, size_t *sampc);

/*
 * account.c
 */
int webapp_accounts_init(void);
void webapp_accounts_close(void);
const struct odict* webapp_accounts_get(void);
void webapp_account_add(const struct odict_entry *acc);
void webapp_account_delete(char *user, char *domain);
void webapp_account_current(void);
void webapp_account_status(const char *aor, bool status);

/*
 * contact.c
 */
int webapp_contacts_init(void);
void webapp_contacts_close(void);
void webapp_contact_add(const struct odict_entry *contact);
void webapp_contact_delete(const char *sip);
const struct odict* webapp_contacts_get(void);

/*
 * option.c
 */
int webapp_options_init(void);
void webapp_options_close(void);
const struct odict* webapp_options_get(void);
void webapp_options_set(char *key, char *value);
char* webapp_options_getv(char *key);

/*
 * chat.c
 */
int webapp_chat_init(void);
void webapp_chat_close(void);
int webapp_chat_add(const char *peer, const char *message, bool self);
int webapp_chat_send(char *message, char *exclude_peer);
const struct odict* webapp_messages_get(void);

/*
 * ws_*.c
 */
void webapp_ws_baresip(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_calls(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_contacts(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_chat(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_meter(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_options(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);
void webapp_ws_meter_init(void);
void webapp_ws_meter_close(void);
int webapp_ws_rtaudio_init(void);
void webapp_ws_rtaudio_close(void);
void webapp_ws_rtaudio(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_rtaudio_sync(void);

/*
 * websocket.c
 */
void ws_send_all(enum ws_type ws_type, char *str);
void ws_send_all_b(enum ws_type ws_type, struct mbuf *mb);
void ws_send_json(enum ws_type type, const struct odict *o);
void srv_websock_close_handler(int err, void *arg);
int webapp_ws_handler(struct http_conn *conn, enum ws_type type,
		                const struct http_msg *msg,
				websock_recv_h *recvh);
void webapp_ws_init(void);
void webapp_ws_close(void);

/*
 * utils.c
 */
void webapp_odict_add(struct odict *og, const struct odict_entry *e);
int webapp_write_file(char *string, char *filename);
int webapp_write_file_json(struct odict *json, char *filename);
int webapp_load_file(struct mbuf *mb, char *filename);
struct call* webapp_get_call(char *sid);
bool webapp_active_calls(void);


/*
 * slaudio module
 */
const struct odict* slaudio_get_interfaces(void);
void slaudio_record_set(bool active);
void slaudio_mono_set(bool active);
void slaudio_mute_set(bool active);

#ifndef SLPLUGIN 
/*
 * session standalone slaudio.h
 */
struct session {
	struct le le;
	struct ausrc_st *st_src;
	struct auplay_st *st_play;
	bool local;
	bool stream;
	bool run_src;
	bool run_play;
	bool run_record;
	int32_t *dstmix;
	struct aubuf *aubuf;
	pthread_t record_thread;
	FLAC__StreamEncoder *flac;
	int16_t *sampv;
	FLAC__int32 *pcm;
	int8_t ch;
	float *vumeter;
	struct call *call;
	int8_t track;
	bool talk;
	int16_t bufsz;
	int16_t jb_max;
	FLAC__StreamMetadata *meta[2];
};
#else
/*
 * session plugin effect.c
 */
struct session {
	struct le le;
	struct ausrc_st *st_src;
	struct auplay_st *st_play;
	uint32_t trev;
	uint32_t prev;
	int32_t *dstmix;
	int8_t ch;
	bool run_src;
	bool run_play;
	struct lock *plock;
	bool run_auto_mix;
	bool bypass;
	struct call *call;
	bool stream; /* only for standalone */
	bool local;  /* only for standalone */
	int8_t track;
	bool talk;
	int16_t bufsz;
	int16_t jb_max;
};
#endif


/*
 * jitter.c
 */
void webapp_jitter(struct session *sess, int16_t *sampv,
		auplay_write_h *wh, unsigned int sampc, void *arg);
