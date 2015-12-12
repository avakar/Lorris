#include "digitaltracegraph.h"
#include <QPaintEvent>
#include <QPainter>

static const int row_header_width = 100;

static size_t clamp(double v)
{
    if (v < 0)
        return 0;
    if (v > std::numeric_limits<size_t>::max())
        return std::numeric_limits<size_t>::max();
    return (size_t)v;
}

DigitalTraceGraph::DigitalTraceGraph(QWidget *parent)
    : QWidget(parent), m_trace_set(0), m_panx(row_header_width*-0.003), m_secondsPerPixel(0.003), m_interpolation(i_point)
{
    this->setMouseTracking(true);
}

void DigitalTraceGraph::setTraceSet(signal_trace_set const * trace_set)
{
    m_trace_set = trace_set;
    this->update();
}

void DigitalTraceGraph::paintEvent(QPaintEvent * event)
{
    QPainter p(this);
    p.fillRect(event->rect(), QColor(0, 0, 64));
    p.setPen(QColor(Qt::white));

    int row_height = 32;
    int row_padding = 2;

    int y = 0;

    signal_trace_set const * const selected_domain = m_trace_set;
    if (!selected_domain)
        return;

    uint64_t first_sample_index = selected_domain->first_sample_index();
    for (int channel_no = 0; channel_no != m_channels.size(); ++channel_no)
    {
        p.setClipping(false);

        auto const & channel = m_channels[channel_no];
        p.drawText(2, y + row_height, channel.name);

        auto trace_range = selected_domain->traces.equal_range(channel.id);
        for (auto trace_it = trace_range.first; trace_it != trace_range.second; ++trace_it)
        {
            signal_trace const & t = trace_it->second;
            size_t channel_length = t.length();
            if (channel_length == 0)
                continue;

            int w = this->width();
            p.setClipRect(row_header_width, 0, w, this->height());

            if (m_secondsPerPixel*t.samples_per_second >= 1)
            {
                bool have_last_sample_pos = false, have_last_ft = false;
                double last_sample_pos = 0;
                std::pair<bool, bool> last_ft;
                for (int x = row_header_width; x < w; ++x)
                {
                    double sample_pos = (x*m_secondsPerPixel+m_panx - t.start_time())*t.samples_per_second + first_sample_index;
                    if (sample_pos < 0)
                        continue;
                    if (sample_pos > channel_length)
                        break;

                    if (have_last_sample_pos)
                    {
                        std::pair<bool, bool> ft = t.multisample(last_sample_pos, sample_pos);
                        if (have_last_ft)
                        {
                            if ((ft.first && ft.second) || (ft.second && !last_ft.second) || (ft.first && !last_ft.first))
                                p.drawLine(x, y + row_padding, x, y + row_height - row_padding);
                            else if (ft.first && last_ft.first)
                                p.drawPoint(x, y + row_height - row_padding);
                            else if (ft.second && last_ft.second)
                                p.drawPoint(x, y + row_padding);
                        }
                        last_ft = ft;
                        have_last_ft = true;
                    }
                    last_sample_pos = sample_pos;
                    have_last_sample_pos = true;
                }
            }
            else
            {
                size_t first_sample = clamp((row_header_width*m_secondsPerPixel+m_panx-t.start_time())*t.samples_per_second+first_sample_index);
                size_t last_sample = clamp((w*m_secondsPerPixel+m_panx - t.start_time())*t.samples_per_second + 2+first_sample_index);
                if (last_sample > t.length())
                    last_sample = t.length();

                if (first_sample > last_sample)
                    first_sample = last_sample;

                QPoint last_point;
                double last_x;
                for (size_t cur = first_sample; cur != last_sample; ++cur)
                {
                    bool sample = t.sample(cur);

                    double x = (((double)cur-first_sample_index)/t.samples_per_second-m_panx+t.start_time())/m_secondsPerPixel;
                    QPoint pt(
                        (int)x,
                        y + (sample? row_padding: row_height - row_padding));

                    if (cur != first_sample)
                    {
                        if (m_interpolation == i_linear || last_point.y() == pt.y())
                        {
                            p.drawLine(last_point, pt);
                        }
                        else
                        {
                            Q_ASSERT(m_interpolation == i_point);

                            int midpoint = (int)((x + last_x) / 2);
                            p.drawLine(last_point.x(), last_point.y(), midpoint, last_point.y());
                            p.drawLine(midpoint, last_point.y(), midpoint, pt.y());
                            p.drawLine(midpoint, pt.y(), pt.x(), pt.y());
                        }
                    }

                    last_point = pt;
                    last_x = x;
                }
            }
        }

        y += row_height;
    }

    p.setClipping(false);
    p.drawText(2, this->height() - 10, QString("seconds/px: %1, panx: %3, x: %2").arg(m_secondsPerPixel, 0, 'f', 15).arg(m_lastCursorPos.x()).arg(m_panx));
}

void DigitalTraceGraph::zoomToAll()
{
    double min_time, max_time;

    signal_trace_set const * const selected_domain = m_trace_set;
    if (!selected_domain || selected_domain->traces.empty())
        return;

    uint64_t first_sample_index = selected_domain->first_sample_index();

    bool first = true;
    for (auto && kv: selected_domain->traces)
    {
        signal_trace const & m = kv.second;
        double start_time = (m.samples_from_epoch-first_sample_index)/m.samples_per_second;
        double end_time = (m.samples_from_epoch+m.length()-first_sample_index)/m.samples_per_second;

        if (first)
        {
            min_time = start_time;
            max_time = end_time;
            first = false;
        }
        else
        {
            min_time = (std::min)(min_time, start_time);
            max_time = (std::max)(max_time, start_time);
        }
    }

    if (first)
        return;

    m_secondsPerPixel = (max_time - min_time) / (this->width() - row_header_width + 1);
    m_panx = min_time - row_header_width*m_secondsPerPixel;
    this->update();
}

void DigitalTraceGraph::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
        m_dragBase = event->pos();
}

void DigitalTraceGraph::mouseMoveEvent(QMouseEvent * event)
{
    m_lastCursorPos = event->pos();
    this->update();

    if (event->buttons().testFlag(Qt::LeftButton))
    {
        QPoint delta = event->pos() - m_dragBase;
        m_panx -= delta.x() * m_secondsPerPixel;
        m_dragBase = event->pos();
        this->update();
    }
}

void DigitalTraceGraph::wheelEvent(QWheelEvent * event)
{
    this->zoom(event->delta(), event->x());
}

void DigitalTraceGraph::zoomIn()
{
    this->zoom(120, this->width() / 2);
}

void DigitalTraceGraph::zoomOut()
{
    this->zoom(-120, this->width() / 2);
}

void DigitalTraceGraph::zoom(int delta, int pivot)
{
    m_panx += pivot * m_secondsPerPixel;
    m_secondsPerPixel *= pow(1.2, -delta/120.0);
    m_panx -= pivot * m_secondsPerPixel;
    this->update();
}

DigitalTraceGraph::interpolation_t DigitalTraceGraph::interpolation() const
{
    return m_interpolation;
}

void DigitalTraceGraph::setInterpolation(interpolation_t v)
{
    m_interpolation = v;
    this->update();
}

void DigitalTraceGraph::addChannel(signal_trace_set::channel_id_t id, QString const & name)
{
    Channel ch;
    ch.id = id;
    ch.name = name;
    m_channels.append(ch);
    this->update();
}

int DigitalTraceGraph::channelCount() const
{
    return m_channels.size();
}

void DigitalTraceGraph::removeChannel(int index)
{
    m_channels.removeAt(index);
    this->update();
}

void DigitalTraceGraph::clearChannels()
{
    m_channels.clear();
    this->update();
}

DigitalTraceGraph::Channel const & DigitalTraceGraph::input(int index)
{
    return m_channels[index];
}
