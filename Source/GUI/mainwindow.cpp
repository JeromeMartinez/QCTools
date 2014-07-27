/*  Copyright (c) BAVC. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QScrollBar>
#include <QSizePolicy>
#include <QScrollArea>
#include <QPrinter>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QShortcut>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QinputDialog>
#include <QCheckBox>
#include <QTimer>

//***************************************************************************
// Constructor / Desructor
//***************************************************************************

//---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // FilesList
    FilesListArea=NULL;

    // CheckBoxes
    for (size_t Pos=0; Pos<PlotType_Max; Pos++)
        CheckBoxes[Pos]=NULL;

    // Plots
    PlotsArea=NULL;

    // Pictures
    TinyDisplayArea=NULL;

    // Control
    ControlArea=NULL;

    // Info
    InfoArea=NULL;

    // Info
    DragDrop_Image=NULL;
    DragDrop_Text=NULL;

    // Files
    Files_CurrentPos=(size_t)-1;

    // UI
    Ui_Init();
}

//---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    // Files (must be deleted first in order to stop ffmpeg processes)
    for (size_t Pos=0; Pos<Files.size(); Pos++)
        delete Files[Pos];    

    // Plots
    delete PlotsArea;

    // Pictures
    delete TinyDisplayArea;

    // Controls
    delete ControlArea;

    // Info
    delete InfoArea;

    // UI
    delete ui;
}

//***************************************************************************
// Actions
//***************************************************************************

//---------------------------------------------------------------------------
void MainWindow::on_actionQuit_triggered()
{
    close();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionOpen_triggered()
{
    openFile();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionClose_triggered()
{
    closeFile();
    if (FilesListArea && ui->actionFilesList->isChecked())
        FilesListArea->UpdateAll();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionCloseAll_triggered()
{
    closeAllFiles();
}

//---------------------------------------------------------------------------
void MainWindow::on_horizontalScrollBar_valueChanged(int value)
{
    Zoom_Move(value);
}

//---------------------------------------------------------------------------
void MainWindow::on_actionZoomIn_triggered()
{
    Zoom_In();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionZoomOut_triggered()
{
    Zoom_Out();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionGoTo_triggered()
{
    if (!ControlArea && !TinyDisplayArea) //TODO: without TinyDisplayArea
        return;

    if (Files_CurrentPos>=Files.size())
        return;
    
    bool ok;
    int i = QInputDialog::getInt(this, tr("Go to frame at position..."), Files[Files_CurrentPos]->Glue->VideoFrameCount?("frame position (0-"+QString::number(Files[Files_CurrentPos]->Glue->VideoFrameCount-1)+"):"):QString("frame position (0-based)"), Files[Files_CurrentPos]->Frames_Pos_Get(), 0, Files[Files_CurrentPos]->Glue->VideoFrameCount-1, 1, &ok);
    if (Files[Files_CurrentPos]->Glue->VideoFrameCount && i>=Files[Files_CurrentPos]->Glue->VideoFrameCount)
        i=Files[Files_CurrentPos]->Glue->VideoFrameCount-1;
    if (ok)
    {
        Files[Files_CurrentPos]->Frames_Pos_Set(i);
    }
}

//---------------------------------------------------------------------------
void MainWindow::on_actionToolbar_triggered()
{
    ui->toolBar->setVisible(ui->actionToolbar->isChecked());
}

//---------------------------------------------------------------------------
void MainWindow::on_Toolbar_visibilityChanged(bool visible)
{
    ui->actionToolbar->setChecked(visible);
}

//---------------------------------------------------------------------------
void MainWindow::on_actionImport_XmlGz_Prompt_triggered()
{
    // TODO
    // Temp
    statusBar()->showMessage("(Not implemeted) Import from "+QFileDialog::getOpenFileName(this, "Import from qctools..xml.gz", QString(), "Statistic files (*.qctools.xml.gz)"));
}

//---------------------------------------------------------------------------
void MainWindow::on_actionExport_XmlGz_Prompt_triggered()
{
    if (Files_CurrentPos>=Files.size() || !Files[Files_CurrentPos])
        return;
    
    // TODO
    //Files[Files_CurrentPos]->Export_CSV(QFileDialog::getSaveFileName(this, "Import from .xml.gz", Files[Files_CurrentPos]->FileName+".qctools.xml.gz", "Statistic files (*.xml.gz)", 0, QFileDialog::DontUseNativeDialog));
    // Temp
    statusBar()->showMessage("(Not implemeted) Export to "+QFileDialog::getSaveFileName(this, "Export to .qctools.xml.gz", Files[Files_CurrentPos]->FileName+".qctools.xml.gz", "Statistic files (*.xml.gz)", 0, QFileDialog::DontUseNativeDialog)+".qctools.xml.gz");
}

//---------------------------------------------------------------------------
void MainWindow::on_actionExport_XmlGz_Sidecar_triggered()
{
    if (Files_CurrentPos>=Files.size() || !Files[Files_CurrentPos])
        return;
    
    // TODO
    // Temp
    statusBar()->showMessage("(Not implemeted) Export to "+Files[Files_CurrentPos]->FileName+".qctools.xml.gz");
}

//---------------------------------------------------------------------------
void MainWindow::on_actionExport_XmlGz_Custom_triggered()
{
    if (Files_CurrentPos>=Files.size() || !Files[Files_CurrentPos])
        return;
    
    // TODO
    // Temp
    QString Name=Files[Files_CurrentPos]->FileName;
    Name.replace(":", "");
    statusBar()->showMessage("(Not implemeted) Export to ~/.qctools"+Name+".qctools.xml.gz");
}

//---------------------------------------------------------------------------
void MainWindow::on_actionCSV_triggered()
{
    Export_CSV();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionPrint_triggered()
{
    Export_PDF();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionFilesList_triggered()
{
    if (ui->actionGoTo)
        ui->actionGoTo->setVisible(false);
    if (ui->actionExport_XmlGz_Prompt)
        ui->actionExport_XmlGz_Prompt->setVisible(false);
    if (ui->actionExport_XmlGz_Sidecar)
        ui->actionExport_XmlGz_Sidecar->setVisible(false);
    if (ui->actionExport_XmlGz_Custom)
        ui->actionExport_XmlGz_Custom->setVisible(false);
    if (ui->actionCSV)
        ui->actionCSV->setVisible(false);
    if (ui->actionPrint)
        ui->actionPrint->setVisible(false);
    if (ui->actionZoomIn)
        ui->actionZoomIn->setVisible(false);
    if (ui->actionZoomOut)
        ui->actionZoomOut->setVisible(false);
    if (ui->actionWindowOut)
        ui->actionWindowOut->setVisible(false);
    for (size_t Pos=0; Pos<PlotType_Max; Pos++)
        if (CheckBoxes[Pos])
            CheckBoxes[Pos]->hide();
    if (ui->fileNamesBox)
        ui->fileNamesBox->hide();
    if (PlotsArea)
        PlotsArea->hide();
    if (TinyDisplayArea)
        TinyDisplayArea->hide();
    if (ControlArea)
        ControlArea->hide();
    if (FilesListArea && !Files.empty())
        FilesListArea->show();

    TimeOut();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionGraphsLayout_triggered()
{
    if (ui->actionGoTo)
        ui->actionGoTo->setVisible(true);
    if (ui->actionExport_XmlGz_Prompt)
        ui->actionExport_XmlGz_Prompt->setVisible(true);
    if (ui->actionExport_XmlGz_Sidecar)
        ui->actionExport_XmlGz_Sidecar->setVisible(true);
    if (ui->actionExport_XmlGz_Custom)
        ui->actionExport_XmlGz_Custom->setVisible(true);
    if (ui->actionCSV)
        ui->actionCSV->setVisible(true);
    if (ui->actionPrint)
        ui->actionPrint->setVisible(true);
    if (ui->actionZoomIn)
        ui->actionZoomIn->setVisible(true);
    if (ui->actionZoomOut)
        ui->actionZoomOut->setVisible(true);
    if (ui->actionWindowOut)
        ui->actionWindowOut->setVisible(false);
    for (size_t Pos=0; Pos<PlotType_Max; Pos++)
        if (CheckBoxes[Pos])
            CheckBoxes[Pos]->show();
    if (ui->fileNamesBox)
        ui->fileNamesBox->show();
    if (PlotsArea)
        PlotsArea->show();
    if (TinyDisplayArea)
        TinyDisplayArea->show();
    if (ControlArea)
        ControlArea->show();
    if (FilesListArea)
        FilesListArea->hide();

    if (ui->fileNamesBox)
        ui->fileNamesBox->setCurrentIndex(Files_CurrentPos);

    TimeOut();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionPreferences_triggered()
{
    Options_Preferences();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionFiltersLayout_triggered()
{
}

//---------------------------------------------------------------------------
void MainWindow::on_actionGettingStarted_triggered()
{
    Help_GettingStarted();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionHowToUseThisTool_triggered()
{
    Help_HowToUse();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionFilterDescriptions_triggered()
{
    Help_FilterDescriptions();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionPlaybackFilters_triggered()
{
    Help_PlaybackFilters();
}

//---------------------------------------------------------------------------
void MainWindow::on_actionAbout_triggered()
{
    Help_About();
}

//---------------------------------------------------------------------------
void MainWindow::on_fileNamesBox_currentIndexChanged(int index)
{
    Files_CurrentPos=index;
    if (!ui->actionGraphsLayout->isChecked())
        return;
    createGraphsLayout();
    refreshDisplay();
    Update();
    QTimer::singleShot(0, this, SLOT(TimeOut_Refresh()));
}

//---------------------------------------------------------------------------
void MainWindow::on_check_toggled(bool checked)
{
    refreshDisplay();
}

//---------------------------------------------------------------------------
void MainWindow::on_M1_triggered()
{
    if (ControlArea)
        ControlArea->on_M1_clicked(true);
}

//---------------------------------------------------------------------------
void MainWindow::on_Minus_triggered()
{
    if (ControlArea)
        ControlArea->on_Minus_clicked(true);
}

//---------------------------------------------------------------------------
void MainWindow::on_PlayPause_triggered()
{
    if (ControlArea)
        ControlArea->on_PlayPause_clicked(true);
}

//---------------------------------------------------------------------------
void MainWindow::on_Pause_triggered()
{
    if (ControlArea)
        ControlArea->on_PlayPause_clicked(true);
}

//---------------------------------------------------------------------------
void MainWindow::on_Plus_triggered()
{
    if (ControlArea)
        ControlArea->on_Plus_clicked(true);
}

//---------------------------------------------------------------------------
void MainWindow::on_P1_triggered()
{
    if (ControlArea)
        ControlArea->on_P1_clicked(true);
}

//---------------------------------------------------------------------------
void MainWindow::on_Full_triggered()
{
  if (isFullScreen())
     setWindowState(Qt::WindowActive);
  else
     setWindowState(Qt::WindowFullScreen);
}

//---------------------------------------------------------------------------
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}
 
//---------------------------------------------------------------------------
void MainWindow::dropEvent(QDropEvent *Event)
{
    const QMimeData* Data=Event->mimeData ();
    if (Event->mimeData()->hasUrls())
    {
        //foreach (QUrl url, Event->mimeData()->urls())
        //clearFiles();
        QList<QUrl> urls=Event->mimeData()->urls();
        for (int Pos=0; Pos<urls.size(); Pos++)
        {
            addFile(urls[Pos].toLocalFile());
        }
    }

    clearDragDrop();
    if (FilesListArea)
    {
        FilesListArea->UpdateAll();
        FilesListArea->show();
    }
    if (Files.size()>1)
        ui->actionFilesList->trigger();
    else
        ui->actionGraphsLayout->trigger();
}

