#ifndef DIGITALTRACEGRAPH_H
#define DIGITALTRACEGRAPH_H

#include "../misc/signal_trace.h"
#include <QWidget>

class DigitalTraceGraph : public QWidget
{
    Q_OBJECT
public:
    enum interpolation_t
    {
        i_point,
        i_linear
    };

    explicit DigitalTraceGraph(QWidget *parent = 0);
    void setTraceSet(signal_trace_set const * trace_set);

    struct Channel
    {
        signal_trace_set::channel_id_t id;
        QString name;
    };

    void addChannel(signal_trace_set::channel_id_t id, QString const & name);
    int channelCount() const;
    void removeChannel(int index);
    void clearChannels();
    Channel const & input(int index);

    interpolation_t interpolation() const;
    void setInterpolation(interpolation_t v);

public slots:
    void zoomToAll();
    void zoomIn();
    void zoomOut();

protected:
    void paintEvent(QPaintEvent * event);
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

private:
    void zoom(int delta, int pivot);

    signal_trace_set const * m_trace_set;
    double m_panx;
    double m_secondsPerPixel;

    double m_sel_start, m_sel_end;

    interpolation_t m_interpolation;

    QPoint m_dragBase;
    QPoint m_lastCursorPos;
    QList<Channel> m_channels;
};

#endif // DIGITALTRACEGRAPH_H
