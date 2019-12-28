#ifndef MYTHREAD_H
#define MYTHREAD_H
#include <QThread>
#include <QString>
#include "re/re.h"
#include "baresip.h"

class MyThread : public QThread
{
public:
    // overriding the QThread's run() method
    void run();
    void quit();
};

#endif // MYTHREAD_H
