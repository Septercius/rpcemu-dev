/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2016-2017 Matthew Howkins

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef MAIN_DISPLAY_H
#define MAIN_DISPLAY_H

#include <QWidget>
#include "rpc-qt6.h"

class MainDisplay : public QWidget
{
    Q_OBJECT

  public:
    MainDisplay(Emulator &emulator, QWidget *parent = 0);

    void get_host_size(int &host_xsize, int &host_ysize) const;
    void set_full_screen(bool full_screen);
    void update_image(const QImage &img, int yl, int yh, int double_size);
    int get_double_size();
    bool save_screenshot(QString filename);

  protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

  private:
    void calculate_scaling();

    Emulator &emulator;

    QImage *image;
    int double_size;

    bool full_screen;
    int host_xsize, host_ysize;
    int scaled_x, scaled_y;
    int offset_x, offset_y;
};

#endif

