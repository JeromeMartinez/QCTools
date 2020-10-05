#include "player.h"
#include "ui_player.h"
#include "Core/FileInformation.h"
#include "Core/FFmpeg_Glue.h"
#include "Core/CommonStats.h"
#include "GUI/filterselector.h"
#include "GUI/Comments.h"
#include "GUI/Plots.h"
#include <cfloat>
#include <QDir>
#include <QAction>
#include <QStandardPaths>
#include <QTimer>
#include <QMetaMethod>
#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QFileDialog>
#include "draggablechildrenbehaviour.h"

const int MaxFilters = 6;
const int DefaultFirstFilterIndex = 0;
const int DefaultSecondFilterIndex = 4;
const int DefaultThirdFilterIndex = 0;
const int DefaultForthFilterIndex = 0;

class ScopedAction
{
public:
    ScopedAction(const std::function<void()>& enterAction = {}, const std::function<void()>& leaveAction = {}) : m_enterAction(enterAction), m_leaveAction(leaveAction) {
        if(m_enterAction)
            m_enterAction();
    }

    ~ScopedAction() {
        if(m_leaveAction)
            m_leaveAction();
    }

    std::function<void()> m_enterAction;
    std::function<void()> m_leaveAction;
};

class ScopedMute
{
public:
    ScopedMute(QtAV::AVPlayer* player) : m_action(nullptr) {
        if(player && player->audio()) {
            m_action = new ScopedAction([this, player] {
                if(!player->audio()->isMute()) {
                    player->audio()->setMute(true);
                    m_muted = true;
                }
            }, [this, player] {
                if(m_muted)
                    player->audio()->setMute(false);
            });
        }
    }

    ~ScopedMute() {
        if(m_action)
            delete m_action;
    }

private:
    bool m_muted = {false};
    ScopedAction* m_action;
};

Player::Player(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Player),
    m_fileInformation(nullptr), m_commentsPlot(nullptr), m_seekOnFileInformationPositionChange(true), m_handlePlayPauseClick(true), m_ignorePositionChanges(false)
{
    QtAV::setLogLevel(QtAV::LogOff);

    ui->setupUi(this);
    m_unit = 1;

    ui->commentsPlaceHolderFrame->setLayout(new QHBoxLayout);
    ui->commentsPlaceHolderFrame->layout()->setMargin(0);

    m_player = new QtAV::AVPlayer(ui->scrollArea);
    m_vo = new QtAV::VideoOutput(ui->scrollArea);
    connect(m_vo, SIGNAL(videoFrameSizeChanged()), this, SLOT(updateVideoOutputSize()));

    ui->scrollArea->setWidget(m_vo->widget());
    ui->scrollArea->widget()->setGeometry(0, 0, 100, 100);

    m_player->setRenderer(m_vo);
    m_player->setSeekType(QtAV::AnyFrameSeek);
    m_player->setMediaEndAction(QtAV::MediaEndAction_Pause);
    m_player->setAsyncLoad(false);
    m_player->setNotifyInterval(10);

    m_videoFilter = new QtAV::LibAVFilterVideo(this);
    m_audioFilter = new QtAV::LibAVFilterAudio(this);

    m_player->installFilter(m_videoFilter);
    m_player->installFilter(m_audioFilter);

    connect(m_player, &QtAV::AVPlayer::stateChanged, [this](QtAV::AVPlayer::State state) {
        if(state == QtAV::AVPlayer::PlayingState) {
            ui->playPause_pushButton->setIcon(QIcon(":/icon/pause.png"));
        } else {
            ui->playPause_pushButton->setIcon(QIcon(":/icon/play.png"));
        }
    });

    connect(ui->playerSlider, SIGNAL(sliderMoved(int)), SLOT(seekBySlider(int)));
    connect(ui->playerSlider, SIGNAL(sliderPressed()), SLOT(seekBySlider()));

    QAction* playAction = new QAction();
    playAction->setShortcuts({ QKeySequence("Space"), QKeySequence("K") });
    connect(playAction, &QAction::triggered, [this]() {
        ui->playPause_pushButton->animateClick();
    });
    addAction(playAction);

    auto* nextAction = new QAction(this);
    nextAction->setShortcuts({ QKeySequence(Qt::Key_Right) });
    connect(nextAction, &QAction::triggered, this, [this]() {
        ui->next_pushButton->animateClick();
    }, Qt::UniqueConnection);
    addAction(nextAction);

    auto* prevAction = new QAction(this);
    prevAction->setShortcuts({ QKeySequence(Qt::Key_Left) });
    connect(prevAction, &QAction::triggered, this, [this]() {
        ui->prev_pushButton->animateClick();
    }, Qt::UniqueConnection);
    addAction(prevAction);

    auto* gotostartAction = new QAction(this);
    gotostartAction->setShortcuts({ QKeySequence(Qt::CTRL + Qt::Key_Left), QKeySequence(Qt::Key_Slash) });
    connect(gotostartAction, &QAction::triggered, this, [this]() {
        ui->goToStart_pushButton->animateClick();
    }, Qt::UniqueConnection);
    addAction(gotostartAction);

    auto* gotoendAction = new QAction(this);
    gotoendAction->setShortcuts({ QKeySequence(Qt::CTRL + Qt::Key_Right), QKeySequence(Qt::Key_BracketRight) });
    connect(gotoendAction, &QAction::triggered, this, [this]() {
        ui->goToEnd_pushButton->animateClick();
    }, Qt::UniqueConnection);
    addAction(gotoendAction);

    connect(m_player, SIGNAL(positionChanged(qint64)), SLOT(updateSlider(qint64)));

    ui->speed_label->installEventFilter(this);

    connect(m_player, &QtAV::AVPlayer::started, [this](){
      // m_player->masterClock()->setClockAuto(false);
      // m_player->masterClock()->setClockType(QtAV::AVClock::ExternalClock);
      updateSlider();
    });

    // connect(m_player, SIGNAL(started()), SLOT(updateSlider()));
    // connect(m_player, SIGNAL(notifyIntervalChanged()), SLOT(updateSliderUnit()));

    connect(ui->arrangementButtonGroup, SIGNAL(buttonToggled(QAbstractButton*, bool)), this, SLOT(applyFilter()));

    ui->filterGroupBox->setLayout(new QVBoxLayout);
    ui->filterGroupBox->layout()->setContentsMargins(2, 2, 2, 2);
    ui->filterGroupBox->setMinimumHeight(50 * MaxFilters);

    static const char* adjustments[] = {
        "Adjust Signal",
        nullptr
    };

    for(int i = 0; i < 6; ++i) {
        m_filterSelectors[i] = new FilterSelector(this, [&](const char* filterName) {
            auto i = 0;
            while(adjustments[i]) {
                if(strcmp(adjustments[i], filterName) == 0)
                    return false;

                ++i;
            }

            return true;
        });

        handleFilterChange(m_filterSelectors[i], i);
        ui->filterGroupBox->layout()->addWidget(m_filterSelectors[i]);
    }

    m_draggableBehaviour = new DraggableChildrenBehaviour(static_cast<QVBoxLayout*> (ui->filterGroupBox->layout()));
    connect(m_draggableBehaviour, &DraggableChildrenBehaviour::childPositionChanged, [&](QWidget* child, int oldPos, int newPos) {
        applyFilter();
    });

    m_adjustmentSelector = new FilterSelector(nullptr, [&](const char* filterName) {
        auto i = 0;
        while(adjustments[i]) {
            if(strcmp(adjustments[i], filterName) == 0)
                return true;

            ++i;
        }

        return false;
    });

    m_adjustmentSelector->setMinimumHeight(50);
    m_adjustmentSelector->selectCurrentFilter(-1);
    m_adjustmentSelector->setCurrentIndex(21);

    handleFilterChange(m_adjustmentSelector, -1);

    ui->adjustmentsGroupBox->setLayout(new QVBoxLayout);
    ui->adjustmentsGroupBox->layout()->setContentsMargins(2, 2, 2, 2);
    ui->adjustmentsGroupBox->layout()->addWidget(m_adjustmentSelector);

    m_filterSelectors[0]->selectCurrentFilter(-1);

    // select 'normal' by default
    m_filterSelectors[0]->enableCurrentFilter(true);

    m_filterUpdateTimer.setSingleShot(true);
    connect(&m_filterUpdateTimer, &QTimer::timeout, this, &Player::applyFilter);
}

Player::~Player()
{
    m_player->stop();
    m_player->removeEventFilter(m_videoFilter);
    m_player->removeEventFilter(m_audioFilter);
    delete ui;
}

FileInformation *Player::file() const
{
    return m_fileInformation;
}

QPushButton *Player::playPauseButton() const
{
    return ui->playPause_pushButton;
}

template <typename T>
class PropertyWaiter {
public:
    PropertyWaiter(const QObject* object, const QString typeName, const QString& propertyName, const T& expectedValue) : _object(object), _propertyName(propertyName), _expectedValue(expectedValue) {
        auto signalName = QString("%1Changed(const %2&)").arg(propertyName).arg(typeName);
        auto emitter = object;

        int index = emitter->metaObject()
                   ->indexOfSignal(QMetaObject::normalizedSignature(qPrintable(signalName)));
        _signal = object->metaObject()->method(index);

        QObject* receiver = &_loop;
        index = receiver->metaObject()
                ->indexOfSlot(QMetaObject::normalizedSignature(qPrintable("quit()")));

        _slot = receiver->metaObject()->method(index);

        QObject::connect(emitter, _signal, receiver, _slot);
    }
    ~PropertyWaiter() {
        auto emitter = _object;
        QObject* receiver = &_loop;

        QObject::disconnect(emitter, _signal, receiver, _slot);
    };

    void wait() {

        while(true) {
            auto propertyValue = _object->property(_propertyName.toStdString().c_str());
            if(qvariant_cast<T>(propertyValue) == _expectedValue)
                return;

            _loop.exec();
        }
    }

private:
    const QObject* _object;
    QMetaMethod _signal;
    QMetaMethod _slot;
    QString _propertyName;
    QEventLoop _loop;
    T _expectedValue;
};

class SignalWaiter {
public:
    SignalWaiter(const QObject* object, const char* signalName, int timeout = -1) : _object(object), _timeout(timeout) {
        auto emitter = object;

        int index = emitter->metaObject()
                   ->indexOfSignal(QMetaObject::normalizedSignature(qPrintable(signalName)));
        _signal = object->metaObject()->method(index);

        QObject* receiver = &_loop;
        index = receiver->metaObject()
                ->indexOfSlot(QMetaObject::normalizedSignature(qPrintable("quit()")));

        _slot = receiver->metaObject()->method(index);

        QObject::connect(emitter, _signal, receiver, _slot);

        if(timeout != -1) {
            _timer.setInterval(timeout);
            _timer.setSingleShot(true);
            QObject::connect(&_timer, &QTimer::timeout, [this]() {
                qDebug() << "SignalWaiter: quit-ing by timeout";
                _loop.quit();
            });
        }

    }

    void wait() {
        if(_timeout != -1)
            _timer.start();

        _loop.exec();
    }

    ~SignalWaiter() {
        auto emitter = _object;
        QObject* receiver = &_loop;

        QObject::disconnect(emitter, _signal, receiver, _slot);

        if(_timeout != -1)
            _timer.stop();
    }
private:
    const QObject* _object;
    QMetaMethod _signal;
    QMetaMethod _slot;
    QEventLoop _loop;
    QTimer _timer;
    int _timeout;
};

void Player::playPaused(qint64 ms)
{
    qDebug() << "play to " << ms;

    ui->playerSlider->setDisabled(true);

    {
        PropertyWaiter<QtAV::AVPlayer::State> waiter(m_player, "QtAV::AVPlayer::State", "state", QtAV::AVPlayer::PlayingState);
        m_player->play();
        waiter.wait();
    }

    QApplication::processEvents();

    {
        PropertyWaiter<QtAV::AVPlayer::State> waiter(m_player, "QtAV::AVPlayer::State", "state", QtAV::AVPlayer::PausedState);
        m_player->pause();
        waiter.wait();
    }

    QApplication::processEvents();

    {
        SignalWaiter waiter(m_player, "seekFinished(qint64)");
        m_player->seek(qint64(ms));
        waiter.wait();        
    }

    if(m_player->displayPosition() > ms)
        m_player->stepBackward();
    else if(m_player->displayPosition() < ms)
        m_player->stepForward();

    ui->playerSlider->setDisabled(false);

    qDebug() << "play to " << ms << " done...";
}

void Player::setFile(FileInformation *fileInfo)
{
    if(fileInfo == nullptr) {
        m_player->stop();
        m_player->setFile(QString());
        m_fileInformation = nullptr;
        return;
    }

    if(m_player->file() != fileInfo->fileName()) {

        if(m_fileInformation != nullptr)
            disconnect(m_fileInformation, &FileInformation::positionChanged, this, &Player::handleFileInformationPositionChanges);

        auto commentsPlot = m_commentsPlot;
        m_commentsPlot = nullptr; // to allow event filter to skip comments plot
        delete commentsPlot;

        m_fileInformation = fileInfo;

        m_commentsPlot = createCommentsPlot(m_fileInformation, nullptr);
        m_commentsPlot->enableAxis(QwtPlot::yLeft, false);
        m_commentsPlot->enableAxis(QwtPlot::xBottom, true);
        m_commentsPlot->setAxisScale(QwtPlot::xBottom, 0, m_fileInformation->Glue->VideoFrameCount_Get());
        m_commentsPlot->setAxisAutoScale(QwtPlot::xBottom, false);

        m_commentsPlot->setFrameShape(QFrame::NoFrame);
        m_commentsPlot->setObjectName("commentsPlot");
        m_commentsPlot->setStyleSheet("#commentsPlot { border: 0px solid transparent; }");
        m_commentsPlot->canvas()->setObjectName("commentsPlotCanvas");
        dynamic_cast<QFrame*>(m_commentsPlot->canvas())->setFrameStyle( QFrame::NoFrame );
        dynamic_cast<QFrame*>(m_commentsPlot->canvas())->setContentsMargins(0, 0, 0, 0);

        connect( m_commentsPlot, SIGNAL( cursorMoved( int ) ), SLOT( onCursorMoved( int ) ) );
        m_commentsPlot->canvas()->installEventFilter( this );
        ui->commentsPlaceHolderFrame->layout()->addWidget(m_commentsPlot);

        for(int i = 0; i < MaxFilters; ++i)
        {
            m_filterSelectors[i]->setFileInformation(m_fileInformation);
        }

        m_filterSelectors[0]->selectCurrentFilterByName("Normal");
        m_filterSelectors[1]->selectCurrentFilterByName("Waveform");
        m_filterSelectors[2]->selectCurrentFilterByName("Bit Plane (10 slices)");
        m_filterSelectors[3]->selectCurrentFilterByName("Vectorscope");

        stopAndWait();

        m_player->setFile(fileInfo->fileName());
        ScopedMute mute(m_player);

        m_player->load();

        m_framesCount = m_fileInformation->Glue->VideoFrameCount_Get();
        ui->playerSlider->setMaximum(m_player->duration());

        m_unit = 1; // qreal(m_player->duration()) / m_framesCount;

        auto ms = frameToMs(m_fileInformation->Frames_Pos_Get());

        playPaused(ms);

        qDebug() << "seek finished at " << ms;

        connect(m_fileInformation, &FileInformation::positionChanged, this, &Player::handleFileInformationPositionChanges);

    }
}

void Player::playPause()
{
    qreal speed = 1.0;
    auto newSpeedInPercent = (double) ui->speedp_horizontalSlider->value();
    speed = newSpeedInPercent / 100;

    m_player->setSpeed(speed);

    if (!m_player->isPlaying()) {
        m_player->play();
        return;
    }
    m_player->pause(!m_player->isPaused());
}

void Player::seekBySlider(int value)
{
    if (!m_player->isPlaying())
        return;

    auto newValue = qint64(value*m_unit);

    auto framePos = msToFrame(newValue);

    m_seekOnFileInformationPositionChange = false;
    m_fileInformation->Frames_Pos_Set(framePos);
    m_seekOnFileInformationPositionChange = true;

    updateInfoLabels();

    qDebug() << "seek to: " << value;

    m_player->seek(newValue);
}

void Player::seekBySlider()
{
    seekBySlider(ui->playerSlider->value());
}

void Player::grabFrame()
{
    ui->export_pushButton->click();
}

void Player::showHideDebug()
{
    if(ui->dockWidget_2->isVisible())
        ui->dockWidget_2->hide();
    else
        ui->dockWidget_2->show();
}

void Player::showHideFilters()
{
    if(ui->dockWidget->isVisible())
        ui->dockWidget->hide();
    else
        ui->dockWidget->show();
}

void Player::showEvent(QShowEvent *event)
{
    updateVideoOutputSize();
}

void Player::resizeEvent(QResizeEvent *event)
{
    updateVideoOutputSize();
}

bool Player::eventFilter(QObject *object, QEvent *event)
{
    if(m_commentsPlot && object == m_commentsPlot->canvas()) {

        if(event->type() == QEvent::MouseButtonDblClick)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if(mouseEvent->button() == Qt::LeftButton)
            {
                showEditFrameCommentsDialog(parentWidget(), m_fileInformation, m_fileInformation->ReferenceStat(), m_fileInformation->Frames_Pos_Get());
            }
        } else if(event->type() == QEvent::KeyPress)
        {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if(keyEvent->key() == Qt::Key_M)
            {
                showEditFrameCommentsDialog(parentWidget(), m_fileInformation, m_fileInformation->ReferenceStat(), m_fileInformation->Frames_Pos_Get());
            }
        }
    } else if(object == ui->speed_label) {

        if(event->type() == QEvent::MouseButtonPress) {
            ui->speedp_horizontalSlider->setValue(100);
        }
    }

    return QWidget::eventFilter( object, event );
}

static QTime zeroTime = QTime::fromString("00:00:00");

void Player::updateInfoLabels()
{
    auto duration = m_player->duration();
    ui->frame_label->setText(QString("Frame %1 [%2]").arg(m_fileInformation->Frames_Pos_Get()).arg(m_fileInformation->Frame_Type_Get()));

    auto framesPos = m_fileInformation->Frames_Pos_Get();

    int Milliseconds=(int)-1;
    if (m_fileInformation && !m_fileInformation->Stats.empty()
     && ( framesPos<m_fileInformation->ReferenceStat()->x_Current
      || (framesPos<m_fileInformation->ReferenceStat()->x_Current_Max && m_fileInformation->ReferenceStat()->x[1][framesPos]))) //Also includes when stats are not ready but timestamp is available
        Milliseconds=(int)(m_fileInformation->ReferenceStat()->x[1][framesPos]*1000);
    else
    {
        double TimeStamp = m_fileInformation->Glue->TimeStampOfCurrentFrame(0);
        if (TimeStamp!=DBL_MAX)
            Milliseconds=(int)(TimeStamp*1000);
    }

    if (Milliseconds >= 0)
    {
        QTime time = zeroTime;
        time = time.addMSecs(Milliseconds);
        QString timeString = time.toString("hh:mm:ss.zzz");
        ui->time_label->setText(timeString);
    }
    else
        ui->time_label->setText("");
}

void Player::updateSlider(qint64 value)
{
    if(m_ignorePositionChanges)
        return;

    auto displayPosition = m_player->displayPosition();
    value = displayPosition;

    auto newValue = int(qreal(value)/m_unit);
    if(ui->playerSlider->value() == newValue)
        return;

    if(!ui->playerSlider->isEnabled() || ui->playerSlider->isSliderDown())
        return;

    // qDebug() << "update slider: " << newValue;

    // ui->playerSlider->setRange(0, int(m_player->duration()/m_unit));
    ui->playerSlider->setValue(newValue);

    auto position = m_player->displayPosition();
    auto framePos = msToFrame(position);

    m_seekOnFileInformationPositionChange = false;
    m_fileInformation->Frames_Pos_Set(framePos);
    m_seekOnFileInformationPositionChange = true;

    auto framesCount = m_fileInformation->Frames_Count_Get();
    // qDebug() << "framesCount: " << framesCount << "framesPos: " << framePos;

    if((framePos + 1) == framesCount) {
        m_player->pause(true);
        m_handlePlayPauseClick = false;
        ui->playPause_pushButton->animateClick(0);
        QTimer::singleShot(0, [&]() {
            m_handlePlayPauseClick = true;
            ui->playPause_pushButton->setIcon(QIcon(":/icon/play.png"));
        });
    }

    updateInfoLabels();
}

void Player::updateSlider()
{
    updateSlider(m_player->displayPosition());
}

void Player::updateSliderUnit()
{
    // m_unit = m_player->notifyInterval();
    // updateSlider();
}

void Player::updateVideoOutputSize()
{
    QSize newSize;

    auto filteredFrameWidth = m_vo->videoFrameSize().width();
    auto filteredFrameHeight = m_vo->videoFrameSize().height();

    if(!ui->fitToScreen_radioButton->isChecked()) {
        double multiplier = ((double) ui->scalePercentage_spinBox->value()) / 100;
        newSize = QSize(filteredFrameWidth, filteredFrameHeight) * multiplier;
    } else {
        auto availableSize = ui->scrollArea->viewport()->size() - QSize(1, 1);

        auto scaleFactor = double(availableSize.width()) / filteredFrameWidth;
        newSize = QSize(availableSize.width(), scaleFactor * filteredFrameHeight);
        if(newSize.height() > availableSize.height()) {
            scaleFactor = double(availableSize.height()) / filteredFrameHeight;
            newSize = QSize(scaleFactor * filteredFrameWidth, availableSize.height());
        }
    }

    auto geometry = ui->scrollArea->widget()->geometry();
    geometry.setSize(newSize);

    ui->scrollArea->widget()->setGeometry(geometry);
}

void Player::applyFilter()
{
    ui->plainTextEdit->clear();

    QStringList definedFilters;
    for(auto i = 0; i < MaxFilters; ++i) {
        auto layoutItem = (ui->filterGroupBox->layout()->itemAt(i));
        auto filter = qobject_cast<FilterSelector*>(layoutItem->widget());
        if(!filter)
            continue;

        auto filterString = replaceFilterTokens(filter->getFilter());
        auto empty = filterString.isEmpty();
        if(!empty)
            definedFilters.append(filterString);
    }

    ui->plainTextEdit->appendPlainText(QString("*** defined filters ***: \n\n%1").arg(definedFilters.join("\n")));

    if(definedFilters.empty()) {
        setFilter(QString());
        return;
    }

    auto layout = QString();
    if(ui->vertical_checkBox->isChecked()) {
        layout = "0_0|0_h0|0_h0+h1|0_h0+h1+h2|0_h0+h1+h2+h3|0_h0+h1+h2+h3+h4";
    } else if(ui->horizontal_checkBox->isChecked()) {
        layout = "0_0|w0_0|w0+w1_0|w0+w1+w2_0|w0+w1+w2+w3_0|w0+w1+w2+w3+w4_0";
    } else if(ui->grid_checkBox->isChecked()) {
        layout = "0_0|w0_0|0_h0|w0_h0|0_h0+h1|w0_h0+h1";
    }

    ui->plainTextEdit->appendPlainText(QString("*** layout ***: \n\n%1").arg(layout));

    QString splits[] = {
        "%1",
        "sws_flags=neighbor;%1split=2[x1][x2];",
        "sws_flags=neighbor;%1split=3[x1][x2][x3];",
        "sws_flags=neighbor;%1split=4[x1][x2][x3][x4];",
        "sws_flags=neighbor;%1split=5[x1][x2][x3][x4][x5];",
        "sws_flags=neighbor;%1split=6[x1][x2][x3][x4][x5][x6];"
    };

    auto adjustmentFilterString = replaceFilterTokens(m_adjustmentSelector->getFilter());
    auto split = splits[definedFilters.length() - 1].arg(!adjustmentFilterString.isEmpty() ? (adjustmentFilterString + ",") : adjustmentFilterString);

    ui->plainTextEdit->appendPlainText(QString("*** split ***: \n\n%1").arg(split));

    QString filterString;

    if(definedFilters.length() == 1) {
        filterString = definedFilters[0];
    } else {
        for(int i = 0; i < definedFilters.length(); ++i) {
            if(ui->fitToGrid_checkBox->isChecked()) {
                filterString += replaceFilterTokens(QString("[x%1]%2,scale=${width}:${height}[y%1];")).arg(i + 1).arg(definedFilters[i]);
            } else {
                filterString += QString("[x%1]%2[y%1];").arg(i + 1).arg(definedFilters[i]);
            }
        }
    }

    ui->plainTextEdit->appendPlainText(QString("*** filterString ***: \n\n%1").arg(filterString));

    QString xstack_inputs[] = {
        "",
        "[y1][y2]",
        "[y1][y2][y3]",
        "[y1][y2][y3][y4]",
        "[y1][y2][y3][y4][y5]",
        "[y1][y2][y3][y4][y5][y6]"
    };

    auto xstack_input = xstack_inputs[definedFilters.length() - 1];

    ui->plainTextEdit->appendPlainText(QString("*** xstack_input ***: \n\n%1").arg(xstack_input));

    QString xstack_option;

    if(definedFilters.length() != 1) {
        xstack_option = QString("%1xstack=fill=slategray:inputs=%2:layout=%3").arg(xstack_input).arg(definedFilters.length()).arg(layout);
    }

    QString combinedFilter = split + filterString + xstack_option;

    if(ui->graphmonitor_checkBox->isChecked())
    {
        combinedFilter.append(QString(",graphmonitor=flags=queue+pts+time+timebase+format+size+rate:m=full"));
    }

    ui->plainTextEdit->appendPlainText(QString("*** result ***: \n\n%1").arg(combinedFilter));

    setFilter(combinedFilter);
}

void Player::handleFileInformationPositionChanges()
{
    if (m_ignorePositionChanges)
        return;

    if(m_player->isPaused() && m_seekOnFileInformationPositionChange) {

        auto ms = frameToMs(m_fileInformation->Frames_Pos_Get());

        if(ms != m_player->displayPosition())
        {
            m_ignorePositionChanges = true;

            auto prevMs = frameToMs(m_fileInformation->Frames_Pos_Get() - 12);
            if(prevMs < 0)
                prevMs = 0;

            ScopedMute mute(m_player);

            SignalWaiter waiter(m_player, "seekFinished(qint64)");
            m_player->seek(qint64(prevMs));
            waiter.wait();

            QApplication::processEvents();

            while(ms > m_player->displayPosition())
            {
                {
                    SignalWaiter waiter(m_player, "stepFinished()", 1000);
                    m_player->stepForward();
                    waiter.wait();
                }
            }
            m_ignorePositionChanges = false;

            ui->playerSlider->setValue(ms);
        }
    }

    m_commentsPlot->setCursorPos(m_fileInformation->Frames_Pos_Get());
}

void Player::onCursorMoved(int x)
{
    m_commentsPlot->setCursorPos(x);
    seekBySlider(frameToMs(x));
}

void Player::on_playPause_pushButton_clicked()
{
    if(m_handlePlayPauseClick)
        playPause();
}

void Player::on_fitToScreen_radioButton_toggled(bool value)
{
    if(value)
    {
        updateVideoOutputSize();
    }
}

void Player::on_normalScale_radioButton_toggled(bool value)
{
    if(value)
    {
        setScaleSliderPercentage(100);
        setScaleSpinboxPercentage(100);
        on_scalePercentage_spinBox_valueChanged(100);
    }
}

void Player::on_scalePercentage_spinBox_valueChanged(int value)
{
    double multiplier = ((double) value) / 100;

    QSize newSize = QSize(m_vo->videoFrameSize().width(), m_vo->videoFrameSize().height()) * multiplier;
    QSize currentSize = ui->scrollArea->widget()->size();

    if(newSize != currentSize)
    {
        if(value != 100 && !ui->freeScale_radioButton->isChecked())
        {
            ui->freeScale_radioButton->blockSignals(true);
            ui->freeScale_radioButton->setChecked(true);
            ui->freeScale_radioButton->blockSignals(false);
        }

        updateVideoOutputSize();
    }

    setScaleSpinboxPercentage(value);
    setScaleSliderPercentage(value);
}

const int MinSliderPercents = 50;
const int MaxSliderPercents = 200;
const int AvgSliderPercents = 100;

void Player::on_scalePercentage_horizontalSlider_valueChanged(int value)
{
    int range = ui->scalePercentage_horizontalSlider->maximum() - ui->scalePercentage_horizontalSlider->minimum();
    int halfRange = range / 2;
    int valueInPercents = 0;

    if(value <= halfRange)
    {
        valueInPercents = (AvgSliderPercents - MinSliderPercents) * (value - ui->scalePercentage_horizontalSlider->minimum()) / halfRange + MinSliderPercents;
    }
    else
    {
        valueInPercents = (MaxSliderPercents -  AvgSliderPercents) * (value - halfRange) / halfRange + AvgSliderPercents;
    }

    on_scalePercentage_spinBox_valueChanged(valueInPercents);
}

void Player::setScaleSliderPercentage(int percents)
{
    ui->scalePercentage_horizontalSlider->blockSignals(true);

    int range = ui->scalePercentage_horizontalSlider->maximum() - ui->scalePercentage_horizontalSlider->minimum();
    int halfRange = range / 2;

    if(percents < MinSliderPercents)
        percents = MinSliderPercents;
    if(percents > MaxSliderPercents)
        percents = MaxSliderPercents;

    if(percents <= AvgSliderPercents) {
        int percentRange = AvgSliderPercents - MinSliderPercents;
        ui->scalePercentage_horizontalSlider->setValue(halfRange * (percents - MinSliderPercents) / percentRange);
    } else {
        int percentRange = MaxSliderPercents - AvgSliderPercents;
        ui->scalePercentage_horizontalSlider->setValue(halfRange + halfRange * (percents - AvgSliderPercents) / percentRange);
    }
    ui->scalePercentage_horizontalSlider->blockSignals(false);
}

void Player::setScaleSpinboxPercentage(int percents)
{
    ui->scalePercentage_spinBox->blockSignals(true);
    ui->scalePercentage_spinBox->setValue(percents);
    ui->scalePercentage_spinBox->blockSignals(false);
}

void Player::handleFilterChange(FilterSelector *filterSelector, int filterIndex)
{
    connect(filterSelector, &FilterSelector::filterChanged, [this, filterIndex](const QString& filterString) {
        m_filterUpdateTimer.stop();
        m_filterUpdateTimer.start(100);
    });
}

void Player::stopAndWait()
{
    {
        PropertyWaiter<QtAV::AVPlayer::State> waiter(m_player, "QtAV::AVPlayer::State", "state", QtAV::AVPlayer::StoppedState);
        m_player->stop();
        waiter.wait();
    }

    QApplication::processEvents();
}

qint64 Player::timeStringToMs(const QString &timeValue)
{
    qint64 ms = 0;

    if(!timeValue.contains(".") && !timeValue.contains(":")) {
        ms = timeValue.toInt();
    } else if (timeValue.contains(".") && !timeValue.contains(":")) {
        auto secAndMs = timeValue.split(".");
        if(secAndMs.count() == 2) {
            auto sec = secAndMs[0].toInt();
            auto msec = secAndMs[1].toInt();
            ms = qint64(sec) * 1000 + msec;
        }
    } else if(!timeValue.contains(".") && timeValue.contains(":")) {
        auto splitted = timeValue.split(":");
        auto hh = 0;
        auto mm = 0;
        auto ss = 0;

        if(splitted.count() == 2) {
            mm = splitted[0].toInt();
            ss = splitted[1].toInt();
        } else if(splitted.count() == 3) {
            hh = splitted[0].toInt();
            mm = splitted[1].toInt();
            ss = splitted[2].toInt();
        }

        ms = qint64(hh) * 60 * 60 * 1000 + qint64(mm) * 60 * 1000 + qint64(ss) * 1000;

    } else if(timeValue.contains(".") && timeValue.contains(":")) {
        auto timeAndMs = timeValue.split(".");
        if(timeAndMs.count() == 2) {
            auto timeValue = timeAndMs[0];
            auto msec = timeAndMs[1].toInt();

            auto splitted = timeValue.split(":");
            auto hh = 0;
            auto mm = 0;
            auto ss = 0;

            if(splitted.count() == 2) {
                mm = splitted[0].toInt();
                ss = splitted[1].toInt();
            } else if(splitted.count() == 3) {
                hh = splitted[0].toInt();
                mm = splitted[1].toInt();
                ss = splitted[2].toInt();
            }

            ms = qint64(hh) * 60 * 60 * 1000 + qint64(mm) * 60 * 1000 + qint64(ss) * 1000 + msec;
        }
    }

    return ms;
}

void Player::setFilter(const QString &filter)
{
    m_videoFilter->setOptions(filter);
    m_audioFilter->setOptions(filter);

    if(m_player->isPaused()) {

        auto sliderValue = (qint64)ui->playerSlider->value();
        qDebug() << "slider value: " << sliderValue;

        stopAndWait();

        playPaused(sliderValue * m_unit);
        QTimer::singleShot(0, this, [&]() {
            updateVideoOutputSize();
        });
    }
}

QString Player::replaceFilterTokens(const QString &filterString)
{
    QString str = filterString;

    str.replace(QString("${width}"), QString::number(m_fileInformation->Glue->Width_Get()));
    str.replace(QString("${height}"), QString::number(m_fileInformation->Glue->Height_Get()));
    str.replace(QString("${dar}"), QString::number(m_fileInformation->Glue->DAR_Get()));

    //    QSize windowSize = imageLabels[Pos]->pixmapSize();
    QSize windowSize = ui->scrollArea->widget()->size();

    str.replace(QString("${window_width}"), QString::number(windowSize.width()));
    str.replace(QString("${window_height}"), QString::number(windowSize.height()));

    QString tempLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir tempDir(tempLocation);

    QString qctoolsTmpSubDir = "qctools";
    QString fontFileName = "Anonymous_Pro_B.ttf";

    if(tempDir.exists())
    {
        QDir qctoolsTmpDir(tempLocation + "/" + qctoolsTmpSubDir);
        if(!qctoolsTmpDir.exists())
            tempDir.mkdir(qctoolsTmpSubDir);

        QFile fontFile(qctoolsTmpDir.path() + "/" + fontFileName);
        if(!fontFile.exists())
        {
            QFile::copy(":/" + fontFileName, fontFile.fileName());
        }

        if(fontFile.exists())
        {
            QString fontFileName(fontFile.fileName());
            fontFileName = fontFileName.replace(":", "\\\\:"); // ":" is a reserved character, it must be escaped
            str.replace(QString("${fontfile}"), fontFileName);
        }
    }

    return str;
}

qint64 Player::frameToMs(int frame)
{
    auto ms = qint64(qreal(m_player->duration()) * frame / m_framesCount);
    return ms;
}

int Player::msToFrame(qint64 ms)
{
    auto frame = ceil(qreal(ms) * m_framesCount / m_player->duration());
    return frame;
}

void Player::on_graphmonitor_checkBox_clicked(bool checked)
{
    applyFilter();
}

void Player::on_goToStart_pushButton_clicked()
{
    qDebug() << "go to start... ";

    ScopedMute mute(m_player);

    {
        SignalWaiter waiter(m_player, "seekFinished(qint64)");
        m_player->seek(qint64(0));
        waiter.wait();
    }

    QApplication::processEvents();

    if(m_player->displayPosition() > 0)
    {
        SignalWaiter waiter(m_player, "stepFinished()", 1000);
        m_player->stepBackward();
        waiter.wait();
    }
    else if(m_player->displayPosition() < 0)
    {
        SignalWaiter waiter(m_player, "stepFinished()", 1000);
        m_player->stepForward();
        waiter.wait();
    }

    qDebug() << "go to start... done. ";
}

void Player::on_goToEnd_pushButton_clicked()
{
    m_player->seek(m_player->startPosition() + m_player->duration());
}

void Player::on_prev_pushButton_clicked()
{
    auto newPosition = m_player->displayPosition() - 1;
    qDebug() << "expected new position: " << newPosition;
    qDebug() << "stepping backward...";
    m_player->stepBackward();
}

void Player::on_next_pushButton_clicked()
{
    auto newPosition = m_player->displayPosition() + 1;
    qDebug() << "expected new position: " << newPosition;
    qDebug() << "stepping forward...";
    m_player->stepForward();
}

void Player::on_fitToGrid_checkBox_toggled(bool checked)
{
    applyFilter();
}

void Player::on_speedp_horizontalSlider_valueChanged(int value)
{
    ui->speed_label->setText(QString("Speed: %1%").arg(value));
    qreal speed = 1.0;
    auto newSpeedInPercent = (double) ui->speedp_horizontalSlider->value();
    speed = newSpeedInPercent / 100;

    m_player->setSpeed(speed);
}

void Player::on_goToTime_lineEdit_returnPressed()
{
    auto timeValue = ui->goToTime_lineEdit->text();
    qint64 ms = timeStringToMs(timeValue);

    qDebug() << "go to " << ms;
    ui->goToTime_lineEdit->clearFocus();

    ui->plainTextEdit->clear();
    ui->plainTextEdit->appendPlainText(QString("*** go to: %1 ***").arg(ms));

    m_player->seek(ms);
}

void Player::on_export_pushButton_clicked()
{
    auto fileName = QFileDialog::getSaveFileName(this, "Export video frame", "", "*.png");
    if(!fileName.isEmpty()) {
        m_player->videoCapture()->setAutoSave(false);
        connect(m_player->videoCapture(), &QtAV::VideoCapture::imageCaptured, this, [&](const QImage& image) {
            image.save(fileName);
        }, Qt::UniqueConnection);

        SignalWaiter waiter(m_player->videoCapture(), "imageCaptured(const QImage&)");
        m_player->videoCapture()->capture();
        waiter.wait();
    }
}
