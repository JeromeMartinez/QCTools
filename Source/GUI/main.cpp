/*  Copyright (c) BAVC. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QtPlugin>
#include <iostream>


#include <QtCore/QtPlugin>
#ifdef __MACOSX__
    #include <ApplicationServices/ApplicationServices.h>
#endif //__MACOSX__

using namespace std;

#include "mainwindow.h"
#include "config.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w(NULL);

    QDesktopWidget desktop;
    auto screenNumber = desktop.screenNumber(&w);
    auto availableGeometry = desktop.availableGeometry(screenNumber);
    auto newSize = availableGeometry.size() * 0.95;
    auto newGeometry = QStyle::alignedRect(Qt::LayoutDirectionAuto, Qt::AlignCenter, newSize, availableGeometry);

    qDebug() << "new size: " << newSize << "availableGeometry: " << availableGeometry << "new geometry: " << newGeometry;
    w.setGeometry(newGeometry);

    for (int Pos=1; Pos<argc; Pos++)
    {
        if(strcmp(argv[Pos], "--debug") == 0)
        {
            Config::instance().setDebug(true);
        }
        else
        {
            w.addFile(QString::fromLocal8Bit(argv[Pos]));
        }
    }
    w.addFile_finish();
    qDebug() << "size: " << w.size() << "pos: " << w.pos();

    w.show();
    qDebug() << "size: " << w.size() << "pos: " << w.pos();

    return a.exec();
}
