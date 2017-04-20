#ifndef RPC_QT5_H
#define RPC_QT5_H

#include <stdint.h>

#include <QtCore>
#include <QKeyEvent>

class Emulator : public QObject {
	Q_OBJECT
public:
	Emulator();
signals:
	//void key_press_signal(int scan_code);
	void key_press_signal(unsigned scan_code);
	//void key_press_signal(uint32_t scan_code);

	void key_release_signal(unsigned scan_code);

	void mouse_move_signal(int x, int y);
	void mouse_press_signal(int buttons);
	void mouse_release_signal(int buttons);

public slots:
	void mainemuloop();

	//void key_press(int scan_code);
	void key_press(unsigned scan_code);
	//void key_press(uint32_t scan_code);

	void key_release(unsigned scan_code);

	void mouse_move(int x, int y);
	void mouse_press(int buttons);
	void mouse_release(int buttons);
};



class VLBUpdateTimer : public QTimer {
  Q_OBJECT
public:
  VLBUpdateTimer(QObject *parent = 0);
private slots:
  void VLBUpdate();
};

#endif /* RPC_QT5_H */
