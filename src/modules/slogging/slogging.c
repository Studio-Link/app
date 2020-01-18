#include <re.h>
#include <baresip.h>
#include <string.h>
#include <time.h>

#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */

static struct http_cli *cli = NULL;
static const struct network *net;
static char url[255] = {0};
static struct http_req *req = NULL;
enum { UUID_LEN = 36 };
static char myid[9] = {0};


static const int lmap[] = { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR };

static void log_handler(uint32_t level, const char *msg)
{
	char fmt[2048] = {0};
	char gelf[2048] = {0};
	int time_ms = 0, timestamp = 0;

	timestamp = time(NULL);

#ifdef WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);
	time_ms = st.wMilliseconds;
#else
	time_ms = (int)(tmr_jiffies() - (uint64_t)timestamp * (uint64_t)1000);
#endif

	re_snprintf(gelf, sizeof(gelf),
		"{\"version\": \"1.1\",\
		\"timestamp\":\"%d.%03d\",\
		\"short_message\":\"%s\",\
		\"host\": \"%s\",\
		\"level\": \"%d\"}\r\n",
		timestamp, time_ms,
		msg,
		myid,
		lmap[MIN(level, ARRAY_SIZE(lmap)-1)]
	);

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


#ifdef WIN32
static void determineWinOsVersion(void)
{
	#define pGetModuleHandle GetModuleHandleW
	typedef LONG NTSTATUS;
	typedef NTSTATUS(NTAPI *RtlGetVersionFunction)(LPOSVERSIONINFO);

	OSVERSIONINFOEX result = { sizeof(OSVERSIONINFOEX),
		0, 0, 0, 0, {'\0'}, 0, 0, 0, 0, 0};
	HMODULE ntdll = pGetModuleHandle(L"ntdll.dll");
	if (!ntdll)
		return;

	RtlGetVersionFunction pRtlGetVersion =
		(RtlGetVersionFunction)GetProcAddress(ntdll, "RtlGetVersion");
	if (!pRtlGetVersion)
		return;

	pRtlGetVersion((LPOSVERSIONINFO)&result);

	info("slogging: Windows Version %i.%i.%i \n", result.dwMajorVersion,
			result.dwMinorVersion, result.dwBuildNumber);
}
#endif

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

	info("slogging: started\n");
	info("slogging: uuid: %s\n", myid);
	info("slogging: Version %s\n", BARESIP_VERSION);
	info("slogging: Machine %s/%s\n", sys_arch_get(), sys_os_get());
	info("slogging: Kernel %H\n", sys_kernel_get, NULL);
	info("slogging: Build %H\n", sys_build_get, NULL);
	info("slogging: Network %H\n", net_debug, net);

#ifdef WIN32
	determineWinOsVersion();
#endif

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
