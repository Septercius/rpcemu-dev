#ifndef NETWORK_DIALOG_H
#define NETWORK_DIALOG_H

#include <QDialog>
#include <QtWidgets>

class NetworkDialog : public QDialog
{
	Q_OBJECT

public:
	NetworkDialog(QWidget *parent = 0);
	virtual ~NetworkDialog();

private slots:
	void radio_clicked();;
	void dialog_accepted();
	void dialog_rejected();

private:
	QRadioButton *net_off, *net_bridging;

	QLabel *bridge_label;
	QLineEdit *bridge_name;
	QHBoxLayout *bridge_hbox;

	QDialogButtonBox *buttons_box;

	QVBoxLayout *vbox;
};

#endif
