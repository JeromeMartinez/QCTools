#ifndef PANELSVIEW_H
#define PANELSVIEW_H

#include <QFrame>
#include <functional>

class PanelsView : public QFrame
{
    Q_OBJECT
public:
    explicit PanelsView(QWidget *parent = nullptr);
    void setProvider(const std::function<int()>& getPanelsCount, const std::function<int()>& getPanelSize,
                     const std::function<QImage(int)>& getPanelImage);

    int panelIndexByFrame(int frameIndex) const;
    void getPanelsBounds(int& startPanelIndex, int& startPanelOffset, int& endPanelIndex, int& endPanelLength);
    void refresh();

public Q_SLOTS:
    void setVisibleFrames(int from, int to);

protected:
    void paintEvent(QPaintEvent *) override;

Q_SIGNALS:

private:
    int m_startFrame;
    int m_endFrame;
    std::function<int()> getPanelsCount;
    std::function<int()> getPanelSize;
    std::function<QImage(int)> getPanelImage;
};

#endif // PANELSVIEW_H
