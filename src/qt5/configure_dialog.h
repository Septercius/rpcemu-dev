#ifndef CONFIGURE_DIALOG_H
#define CONFIGURE_DIALOG_H

#include <QDialog>
#include <QtWidgets>

class ConfigureDialog : public QDialog
{
	Q_OBJECT

public:
	ConfigureDialog(QWidget *parent = 0);
	virtual ~ConfigureDialog();

	void keyPressEvent(QKeyEvent *);

private slots:
	void slider_moved(int value);

	void dialog_accepted();
	void dialog_rejected();

private:
	QListWidget *hardware_listwidget;
	QVBoxLayout *hardware_vbox;
	QGroupBox *hardware_group_box;

	QButtonGroup *mem_group;
	QRadioButton *mem_4, *mem_8, *mem_16, *mem_32, *mem_64, *mem_128, *mem_256;
	QVBoxLayout *mem_vbox;
	QGroupBox *mem_group_box;

	QButtonGroup *vram_group;
	QRadioButton *vram_0, *vram_2;
	QVBoxLayout *vram_vbox;
	QGroupBox *vram_group_box;

	QCheckBox *sound_checkbox;

	QSlider *refresh_slider;
	QLabel *refresh_label;
	QHBoxLayout *refresh_hbox;
	QGroupBox *refresh_group_box;

	QDialogButtonBox *buttons_box;

	QGridLayout *grid;
};

#endif
