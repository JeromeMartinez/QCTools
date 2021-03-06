#ifndef PLAYER_H
#define PLAYER_H

#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <QtAV>

class FileInformation;
class FilterSelector;
class DraggableChildrenBehaviour;
class CommentsPlot;

namespace Ui {
class Player;
}

class Player : public QMainWindow
{
    Q_OBJECT

public:
    explicit Player(QWidget *parent = nullptr);
    ~Player();

    FileInformation* file() const;

    QPushButton* playPauseButton() const;
    void playPaused(qint64 ms);

    void updateInfoLabels();

    void stopAndWait();

    static qint64 timeStringToMs(const QString& timeValue);

public Q_SLOTS:
    void setFile(FileInformation* filePath);
    void playPause();
    void seekBySlider(int value);
    void seekBySlider();
    void grabFrame();
    void showHideDebug();
    void showHideFilters();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter( QObject *object, QEvent *event ) override;

private Q_SLOTS:
    void updateSlider(qint64 value);
    void updateSlider();
    void updateSliderUnit();
    void updateVideoOutputSize();
    void applyFilter();
    void handleFileInformationPositionChanges();
    void onCursorMoved(int index);

private Q_SLOTS:
    void on_playPause_pushButton_clicked();

    void on_fitToScreen_radioButton_toggled(bool value);

    void on_normalScale_radioButton_toggled(bool value);

    void on_scalePercentage_spinBox_valueChanged(int value);

    void on_scalePercentage_horizontalSlider_valueChanged(int value);

    void on_graphmonitor_checkBox_clicked(bool checked);

    void on_goToStart_pushButton_clicked();

    void on_goToEnd_pushButton_clicked();

    void on_prev_pushButton_clicked();

    void on_next_pushButton_clicked();

    void on_fitToGrid_checkBox_toggled(bool checked);

    void on_speedp_horizontalSlider_valueChanged(int value);

    void on_goToTime_lineEdit_returnPressed();

    void on_export_pushButton_clicked();

private:
    void setScaleSliderPercentage(int percents);
    void setScaleSpinboxPercentage(int percents);
    void handleFilterChange(FilterSelector *filterSelector, int filterIndex);
    void setFilter(const QString& filter);
    QString replaceFilterTokens(const QString& filterString);

private:
    qint64 frameToMs(int frame);
    int msToFrame(qint64 ms);

private:
    Ui::Player *ui;

    QtAV::VideoOutput *m_vo;
    QtAV::AVPlayer *m_player;
    bool m_handlePlayPauseClick;

    qreal m_unit;
    int m_framesCount;

    QtAV::LibAVFilterVideo* m_videoFilter;
    QtAV::LibAVFilterAudio* m_audioFilter;

    FileInformation* m_fileInformation;
    FilterSelector* m_filterSelectors[6];
    FilterSelector* m_adjustmentSelector;
    DraggableChildrenBehaviour* m_draggableBehaviour;
    CommentsPlot* m_commentsPlot;
    bool m_seekOnFileInformationPositionChange;

    QTimer m_filterUpdateTimer;
};

#endif // PLAYER_H
