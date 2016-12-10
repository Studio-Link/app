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

int webapp_call_delete(struct call *call);
int webapp_call_update(struct call *call, char *state);

/*
 * vumeter.c
 */
int webapp_vu_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm);
int webapp_vu_decode_update(struct aufilt_dec_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm);
int webapp_vu_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc);
int webapp_vu_decode(struct aufilt_dec_st *st, int16_t *sampv, size_t *sampc);

/*
 * mono.c
 */
int webapp_mono_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm);
int webapp_mono_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc);
void webapp_mono_set(bool active);

/*
 * record.c
 */
int webapp_record_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm);
int webapp_record_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc);
void webapp_record_set(bool active);

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
const struct odict* webapp_messages_get(void);

/*
 * ws_*.c
 */
void webapp_ws_baresip(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_calls(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg);
void webapp_ws_call_mute_send(void);
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
