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
	void key_press_signal(unsigned scan_code);

	void key_release_signal(unsigned scan_code);

	void mouse_move_signal(int x, int y);
	void mouse_press_signal(int buttons);
	void mouse_release_signal(int buttons);

	void reset_signal();
	void load_disc_0_signal(const QString &discname);
	void load_disc_1_signal(const QString &discname);
	void cdrom_disabled_signal();
	void cdrom_empty_signal();
	void cdrom_load_iso_signal(const QString &discname);

public slots:
	void mainemuloop();

	void key_press(unsigned scan_code);

	void key_release(unsigned scan_code);

	void mouse_move(int x, int y);
	void mouse_press(int buttons);
	void mouse_release(int buttons);

	void reset();
	void load_disc_0(const QString &discname);
	void load_disc_1(const QString &discname);
	void cdrom_disabled();
	void cdrom_empty();
	void cdrom_load_iso(const QString &discname);
};



class VLBUpdateTimer : public QTimer {
  Q_OBJECT
public:
  VLBUpdateTimer(QObject *parent = 0);
private slots:
  void VLBUpdate();
};

#endif /* RPC_QT5_H */
