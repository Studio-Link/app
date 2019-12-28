#include "baresipthread.h"
#include <QDebug>

void BaresipThread::run()
{
    libre_init();

    (void)sys_coredump_set(true);
    conf_configure(false);
    baresip_init(conf_config());
    ua_init("Studio Link v" BARESIP_VERSION " (" ARCH "/" OS ")",
            true, true, true);
    conf_modules();
    re_main(NULL);
}

void BaresipThread::quit()
{
    ua_stop_all(false);
    sys_msleep(800);
    ua_close();
    re_cancel();
    conf_close();
    baresip_close();
    mod_close();
    libre_close();
}
