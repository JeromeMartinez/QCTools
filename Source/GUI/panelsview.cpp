#include "panelsview.h"
#include <QDebug>
#include <QPainter>

PanelsView::PanelsView(QWidget *parent) : QFrame(parent)
{

}

void PanelsView::setProvider(const std::function<int ()> &getPanelsCount, const std::function<int ()> &getPanelSize, const std::function<QImage (int)> &getPanelImage)
{
    this->getPanelsCount = getPanelsCount;
    this->getPanelSize = getPanelSize;
    this->getPanelImage = getPanelImage;
}

int PanelsView::panelIndexByFrame(int frameIndex) const
{
    auto panelIndex = ceil((double) frameIndex / getPanelSize());
    return panelIndex;
}

void PanelsView::getPanelsBounds(int &startPanelIndex, int &startPanelOffset, int &endPanelIndex, int &endPanelLength)
{
    auto panelSize = getPanelSize();

    startPanelOffset = m_startFrame % panelSize;
    startPanelIndex = (m_startFrame - startPanelOffset) / panelSize;

    endPanelLength = m_endFrame % panelSize;
    endPanelIndex = (m_endFrame - endPanelLength) / panelSize;
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

void PanelsView::paintEvent(QPaintEvent *)
{
    QPainter p;
    p.begin(this);

    int startPanelOffset, startPanelIndex, endPanelLength, endPanelIndex;
    getPanelsBounds(startPanelIndex, startPanelOffset, endPanelIndex, endPanelLength);

    qDebug() << "startPanelIndex: " << startPanelIndex << "startPanelOffset: " << startPanelOffset
             << "endPanelIndex: " << endPanelIndex << "endPanelLength: " << endPanelLength << "width: " << width();

    // p.fillRect(QRect(0, 0, width(), height()), Qt::green);
    int x = contentsMargins().left();
    int y = 0;

    for(auto i = startPanelIndex; i <= endPanelIndex; ++i) {

        if(i < getPanelsCount())
        {
            auto image = getPanelImage(i);
            auto imageXOffset = ((i == startPanelIndex) ? startPanelOffset : 0);
            auto imageWidth = ((i == endPanelIndex) ? endPanelLength : image.width());

            QRect sr(imageXOffset, 100, imageWidth, height());
            p.drawImage(QPointF(x, y), image, sr);
            //p.fillRect(x, 0, imageWidth, image.height(), Qt::red);

            qDebug() << "x: " << x << "sr: " << sr;
            x += (sr.width() - imageXOffset);
        }
    }

    p.end();
}
