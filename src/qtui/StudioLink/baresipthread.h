#ifndef BARESIPTHREAD_H
#define BARESIPTHREAD_H
#include <QThread>
#include <QString>
#ifdef SL_BARESIP_BACKEND
#include "re/re.h"
#include "baresip.h"
#endif

class BaresipThread : public QThread
{
public:
    // overriding the QThread's run() method
    void run();
    void quit();
};

#endif // BARESIPTHREAD_H
