#ifndef BARESIPTHREAD_H
#define BARESIPTHREAD_H
#include <QThread>
#include <QString>
#include "re/re.h"
#include "baresip.h"

class BaresipThread : public QThread
{
public:
    // overriding the QThread's run() method
    void run();
    void quit();
};

#endif // BARESIPTHREAD_H
