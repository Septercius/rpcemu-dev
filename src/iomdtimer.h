#ifndef IOMDTIMER_H
#define IOMDTIMER_H

#include <QtCore>



class IOMDTimer : public QTimer {
  Q_OBJECT
public:
  IOMDTimer(QObject *parent = 0);
private slots:
  void IOMDUpdate();
};

#endif /* IOMDTIMER_H */
