#include "panelsview.h"
#include <QDebug>
#include <QPainter>

PanelsView::PanelsView(QWidget *parent) : QFrame(parent), m_actualWidth(0)
{

}

void PanelsView::setProvider(const std::function<int ()> &getPanelsCount, const std::function<QSize ()> &getPanelSize, const std::function<QImage (int)> &getPanelImage)
{
    this->getPanelsCount = getPanelsCount;
    this->getPanelSize = getPanelSize;
    this->getPanelImage = getPanelImage;
}

int PanelsView::panelIndexByFrame(int frameIndex) const
{
    auto panelIndex = ceil((double) frameIndex / getPanelSize().width());
    return panelIndex;
}

void PanelsView::getPanelsBounds(int &startPanelIndex, int &startPanelOffset, int &endPanelIndex, int &endPanelLength)
{
    auto panelSize = getPanelSize();
    auto panelWidth = panelSize.width();

    startPanelOffset = m_startFrame % panelWidth;
    startPanelIndex = (m_startFrame - startPanelOffset) / panelWidth;

    endPanelLength = m_endFrame % panelWidth;
    endPanelIndex = (m_endFrame - endPanelLength) / panelWidth;
}

void PanelsView::refresh()
{
    repaint();
}

void PanelsView::setVisibleFrames(int from, int to)
{
    m_startFrame = from;
    m_endFrame = to;
    qDebug() << "from: " << from << "to: " << to;

    repaint();
}

void PanelsView::setActualWidth(int actualWidth)
{
    m_actualWidth = actualWidth;
}

void PanelsView::paintEvent(QPaintEvent *)
{
    QPainter p;
    p.begin(this);

    auto availableWidth = width() - contentsMargins().left() - contentsMargins().right();
    auto sx = m_actualWidth == 0 ? 1 : ((qreal) availableWidth / m_actualWidth);
    auto sy = (qreal)height() / getPanelSize().height();

    auto dx = availableWidth - m_actualWidth;
    qDebug() << "sx: " << sx << "m_actualWidth: " << m_actualWidth;

    QRect viewport(contentsMargins().left(), 0, width() - contentsMargins().right(), height());
    p.setViewport(viewport);

    qDebug() << "viewport: " << viewport;

    QMatrix scalingMatrix(sx, 0, 0, sy, 0, 0);
    QMatrix translationMatrix(1, 0, 0, 1, 0, 0);
    p.setMatrix(scalingMatrix * translationMatrix);

    int startPanelOffset, startPanelIndex, endPanelLength, endPanelIndex;
    getPanelsBounds(startPanelIndex, startPanelOffset, endPanelIndex, endPanelLength);

    qDebug() << "contentsMargins: " << contentsMargins();

    qDebug() << "startPanelIndex: " << startPanelIndex << "startPanelOffset: " << startPanelOffset
             << "endPanelIndex: " << endPanelIndex << "endPanelLength: " << endPanelLength << "width: " << width() << "actual: " << m_actualWidth
             << "height: " << height();

    // p.fillRect(QRect(0, 0, width(), height()), Qt::green);
    int x = 0;
    int y = 0;

    for(auto i = startPanelIndex; i <= endPanelIndex; ++i) {

        if(i < getPanelsCount())
        {
            auto image = getPanelImage(i);
            auto imageXOffset = ((i == startPanelIndex) ? startPanelOffset : 0);
            auto imageWidth = ((i == endPanelIndex) ? endPanelLength : image.width());
            auto imageHeight = image.height();

            QRect sr(imageXOffset, 0, imageWidth, imageHeight);
            p.drawImage(QPointF(x, y), image, sr);
            //p.fillRect(x, 0, imageWidth, image.height(), Qt::red);

            qDebug() << "x: " << x << "sr: " << sr;
            x += (sr.width() - imageXOffset);
        }
    }

    p.end();
}