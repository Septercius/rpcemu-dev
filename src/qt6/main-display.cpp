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

#include <QPainter>

#include "main-display.h"
#include "rpc-qt6.h"
#include "rpcemu.h"
#include "vidc20.h"

MainDisplay::MainDisplay(Emulator &emulator, QWidget *parent)
  : QWidget(parent)
  , emulator(emulator)
  , double_size(VIDC_DOUBLE_NONE)
  , full_screen(false)
{
    assert(pconfig_copy);

    image = new QImage(640, 480, QImage::Format_RGB32);

    // No need to erase to background colour before painting
    this->setAttribute(Qt::WA_OpaquePaintEvent);

    // Hide pointer in mouse hack mode
    if (pconfig_copy->mousehackon)
    {
        this->setCursor(Qt::BlankCursor);
    }

    calculate_scaling();
}

void MainDisplay::mouseMoveEvent(QMouseEvent *event)
{
    if (terminating)
    {
        return;
    }

    if ((!pconfig_copy->mousehackon && mouse_captured) || full_screen)
    {
        QPoint middle;

        // In mouse capture mode move the mouse back to the middle of the window */
        middle.setX(this->width() / 2);
        middle.setY(this->height() / 2);

        QCursor::setPos(this->mapToGlobal(middle));

        // Calculate relative deltas based on difference from centre of display widget
        int dx = event->position().x() - middle.x();
        int dy = event->position().y() - middle.y();

        emit this->emulator.mouse_move_relative_signal(dx, dy);
    }
    else if (pconfig_copy->mousehackon)
    {
        // Follows host mouse (mousehack) mode
        emit this->emulator.mouse_move_signal(event->position().x(), event->position().y());
    }
}

void MainDisplay::mousePressEvent(QMouseEvent *event)
{
    if (terminating)
    {
        return;
    }

    // Handle turning on mouse capture in capture mouse mode
    if (!pconfig_copy->mousehackon)
    {
        if (!mouse_captured)
        {
            mouse_captured = 1;

            // Hide pointer in mouse capture mode when it's been captured
            this->setCursor(Qt::BlankCursor);

            return;
        }
    }

    if (event->button() & 7)
    {
        emit this->emulator.mouse_press_signal(event->button() & 7);
    }
}

void MainDisplay::mouseReleaseEvent(QMouseEvent *event)
{
    if (terminating)
    {
        return;
    }

    if (event->button() & 7)
    {
        emit this->emulator.mouse_release_signal(event->button() & 7);
    }
}

void MainDisplay::wheelEvent(QWheelEvent *event)
{
    if (terminating)
    {
        return;
    }

    const int dy = event->angleDelta().y();

    emit this->emulator.mouse_wheel_signal(dy);
}

void MainDisplay::paintEvent(QPaintEvent *event)
{
    if (terminating)
    {
        return;
    }

    QPainter painter(this);

    if (full_screen)
    {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    else
    {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }

    const QRect dest = event->rect();
    QRect source;

    switch (double_size)
    {
        case VIDC_DOUBLE_NONE:
            source = dest;
            break;
        case VIDC_DOUBLE_X:
            source = QRect(dest.x() / 2, dest.y(), dest.width() / 2, dest.height());
            break;
        case VIDC_DOUBLE_Y:
            source = QRect(dest.x(), dest.y() / 2, dest.width(), dest.height() / 2);
            break;
        case VIDC_DOUBLE_BOTH:
            source = QRect(dest.x() / 2, dest.y() / 2, dest.width() / 2, dest.height() / 2);
            break;
    }

    if (full_screen)
    {
        if ((dest.x() < offset_x) || (dest.y() < offset_y))
        {
            painter.fillRect(dest, Qt::black);
        }

        const QRect rect(offset_x, offset_y, scaled_x, scaled_y);
        painter.drawImage(rect, *image);
    }
    else
    {
        painter.drawImage(dest, *image, source);
    }
}

void MainDisplay::resizeEvent(QResizeEvent *)
{
    if (terminating)
    {
        return;
    }

    calculate_scaling();
}

void MainDisplay::get_host_size(int &xsize, int &ysize) const
{
    xsize = this->host_xsize;
    ysize = this->host_ysize;
}

void MainDisplay::set_full_screen(bool full_screen)
{
    this->full_screen = full_screen;

    calculate_scaling();
}

void MainDisplay::update_image(const QImage &img, int yl, int yh, int double_size)
{
    bool recalculate_needed = false;

    if (img.size() != image->size())
    {
        // Re-create image with new size and copy of data
        *(this->image) = img.copy();

        recalculate_needed = true;
    }
    else
    {
        // Copy just the data that has changed
        const void *src = img.scanLine(yl);
        void *dest = image->scanLine(yl);

        const int lines = yh - yl;
        const int bytes = (int) img.bytesPerLine() * lines;

        memcpy(dest, src, (size_t) bytes);
    }

    if (double_size != this->double_size)
    {
        this->double_size = double_size;
        recalculate_needed = true;
    }

    if (recalculate_needed)
    {
        calculate_scaling();
        this->update();
        return;
    }

    // Trigger repaint of changed region
    int width = image->width();
    int ymin = yl;
    int ymax = yh;

    if (double_size & VIDC_DOUBLE_X)
    {
        width *= 2;
    }
    if (double_size & VIDC_DOUBLE_Y)
    {
        ymin *= 2;
        ymax *= 2;
    }

    if (full_screen)
    {
        width = (width * scaled_x) / host_xsize;

        /* For the Pixmap Smoothing to work properly, the height
         * needs to be expanded by one pixel to avoid visual
         * artifacts */
        if (ymin > 0)
        {
            ymin--;
        }
        if (ymax < host_ysize)
        {
            ymax++;
        }

        // calculate 'ymin' rounded down, 'ymax' rounded up
        ymin = (ymin * scaled_y) / host_ysize;
        ymax = ((ymax * scaled_y) + host_ysize - 1) / host_ysize;

        int height = ymax - ymin;
        this->update(offset_x, ymin + offset_y, width, height);
    }
    else
    {
        int height = ymax - ymin;
        this->update(0, ymin, width, height);
    }
}

/**
 * Called to update the image scaling.
 *
 * Called when any of the following change:
 * - Image size
 * - Double-size
 * - Windowed or Full screen
 * - Widget size
 */
void MainDisplay::calculate_scaling()
{
    if (double_size & VIDC_DOUBLE_X)
    {
        host_xsize = image->width() * 2;
    }
    else
    {
        host_xsize = image->width();
    }
    if (double_size & VIDC_DOUBLE_Y)
    {
        host_ysize = image->height() * 2;
    }
    else
    {
        host_ysize = image->height();
    }

    if (full_screen)
    {
        const int widget_x = this->width();
        const int widget_y = this->height();

        if ((widget_x * host_ysize) >= (widget_y * host_xsize))
        {
            scaled_x = (widget_y * host_xsize) / host_ysize;
            scaled_y = widget_y;
        }
        else
        {
            scaled_x = widget_x;
            scaled_y = (widget_x * host_ysize) / host_xsize;
        }

        offset_x = (widget_x - scaled_x) / 2;
        offset_y = (widget_y - scaled_y) / 2;
    }
}

/**
 * Is the display currently doubling in either direction
 * needed in MainWindow to adjust mouse coordinates from the emulator
 */
int MainDisplay::get_double_size()
{
    return double_size;
}

/**
 * Dump the current display to the file specified.
 *
 * @param filename Filename to save display image to
 *
 * @return bool of success or failure
 */
bool MainDisplay::save_screenshot(QString filename)
{
    return this->image->save(filename, "png");
}
