#include <re.h>
#include <baresip.h>
#include <string.h>

static struct http_cli *cli = NULL;
static const struct network *net;
static char url[255] = {0};
static struct http_req *req = NULL;
enum { UUID_LEN = 36 };
static char myid[9] = {0};


static void log_handler(uint32_t level, const char *msg)
{
    char fmt[1024] = {0};
    char gelf[1024] = {0};

    re_snprintf(gelf, sizeof(gelf), 
        "{\"short_message\":\"%s\", \"host\": \"%s\"}\r\n", msg, myid);

    re_snprintf(fmt, sizeof(fmt), 
        "Content-Length: %d\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "\r\n"
	"%s", 
        str_len(gelf), gelf);

    http_request(&req, cli, "POST", url, NULL,
                 NULL, NULL, fmt);
}


static struct log lg = {
    .h = log_handler,
};


static int uuid_load(const char *file, char *uuid, size_t sz)
{
	FILE *f = NULL;
	int err = 0;

	f = fopen(file, "r");
	if (!f)
		return errno;

	if (!fgets(uuid, (int)sz, f))
		err = errno;

	(void)fclose(f);

	debug("uuid: loaded UUID %s from file %s\n", uuid, file);

	return err;
}


static int generate_random_uuid(FILE *f)
{
	if (re_fprintf(f, "%08x-%04x-%04x-%04x-%08x%04x",
		       rand_u32(), rand_u16(), rand_u16(), rand_u16(),
		       rand_u32(), rand_u16()) != UUID_LEN)
		return ENOMEM;

	return 0;
}


static int uuid_init(const char *file)
{
	FILE *f = NULL;
	int err = 0;

	f = fopen(file, "r");
	if (f) {
		err = 0;
		goto out;
	}

	f = fopen(file, "w");
	if (!f) {
		err = errno;
		warning("uuid: fopen() %s (%m)\n", file, err);
		goto out;
	}

	err = generate_random_uuid(f);
	if (err) {
		warning("uuid: generate random UUID failed (%m)\n", err);
		goto out;
	}

	info("uuid: generated new UUID in %s\n", file);

 out:
	if (f)
		fclose(f);

	return err;
}


static int module_init(void)
{
    int err = 0;
	char path[256];
    char uuidtmp[40];

	err = conf_path_get(path, sizeof(path));
	if (err)
		return err;

	strncat(path, "/uuid", sizeof(path) - strlen(path) - 1);

	err = uuid_init(path);
	if (err)
		return err;

	err = uuid_load(path, uuidtmp, sizeof(uuidtmp));
	if (err)
		return err;

    str_ncpy(myid, uuidtmp, sizeof(myid));

    net = baresip_network();
    re_snprintf(url, sizeof(url), "https://log.studio.link/gelf");
    http_client_alloc(&cli, net_dnsc(net));

    log_register_handler(&lg);

    return 0;
}

static int module_close(void)
{
    log_unregister_handler(&lg);
	req = mem_deref(req);
	cli = mem_deref(cli);

    return 0;
}

const struct mod_export DECL_EXPORTS(slogging) = {
	"slogging",
	"application",
	module_init,
	module_close
};
