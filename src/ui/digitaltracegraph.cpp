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
    : QWidget(parent), m_trace_set(0), m_panx(row_header_width*-0.003), m_secondsPerPixel(0.003), m_sel_start(0), m_sel_end(0), m_interpolation(i_point)
{
    this->setMouseTracking(true);
}

void DigitalTraceGraph::setTraceSet(signal_trace_set const * trace_set)
{
    m_trace_set = trace_set;
    this->update();
}

static QString timeToString(double time_s, int time_scale_rank)
{
    QString res;

    if (time_scale_rank <= -12)
    {
        res = "%1 ps";
        time_s *= 1e12;
    }
    else if (time_scale_rank <= -9)
    {
        res = "%1 ns";
        time_s *= 1e9;
    }
    else if (time_scale_rank <= -6)
    {
        res = "%1 us";
        time_s *= 1e6;
    }
    else if (time_scale_rank <= -3)
    {
        res = "%1 ms";
        time_s *= 1e3;
    }
    else
    {
        res = "%1 s";
    }

    return res.arg(time_s, 0, 'f', 3);
}

void DigitalTraceGraph::paintEvent(QPaintEvent * event)
{
    static const int max_square_px = 200;

    QPainter p(this);
    p.fillRect(event->rect(), QColor(0, 0, 64));

    int sel_left = (m_sel_start - m_panx) / m_secondsPerPixel;
    int sel_right = (m_sel_end - m_panx) / m_secondsPerPixel;

    if (sel_left > sel_right)
        std::swap(sel_left, sel_right);
    if (sel_left < 0)
        sel_left = 0;
    if (sel_right > this->width())
        sel_right = this->width();
    p.fillRect(sel_left, 0, sel_right - sel_left, this->height(), QColor(32, 32, 64));

    int square_rank = (int)floor(log10(m_secondsPerPixel * max_square_px));
    double square_size = pow(10, square_rank);

    double right_sq = ceil((this->width()*m_secondsPerPixel + m_panx) / square_size);
    for (double cur = floor(m_panx / square_size); cur <= right_sq; cur += 1)
    {
        int x = (int)((cur*square_size - m_panx)/m_secondsPerPixel);
        if (x > row_header_width) {
            bool major = (int)cur % 10 == 0;
            p.setPen(major? QColor(128, 128, 128): QColor(64, 64, 64));
            p.drawLine(x, 0, x, this->height());
        }
    }

    p.setPen(Qt::white);
    if (m_sel_start == m_sel_end)
    {
        p.drawText(2, this->height() - 10, QString("%1").arg(timeToString(m_lastCursorPos.x()*m_secondsPerPixel+m_panx, square_rank)));
    }
    else
    {
        p.drawText(2, this->height() - 10, QString("%1, %2")
            .arg(timeToString(m_lastCursorPos.x()*m_secondsPerPixel+m_panx, square_rank))
            .arg(timeToString(std::abs(m_sel_end - m_sel_start), square_rank)));
    }

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

        p.setPen(QColor(0, 0, 128));
        p.drawLine(row_header_width, y + row_height, this->width(), y + row_height);
        p.setPen(QColor(Qt::white));

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
    {
        m_sel_start = m_sel_end = event->pos().x() * m_secondsPerPixel + m_panx;
        this->update();
    }

    if (event->button() == Qt::MiddleButton)
        m_dragBase = event->pos();
}

void DigitalTraceGraph::mouseMoveEvent(QMouseEvent * event)
{
    m_lastCursorPos = event->pos();
    this->update();

    if (event->buttons().testFlag(Qt::LeftButton))
    {
        m_sel_end = event->pos().x() * m_secondsPerPixel + m_panx;
    }

    if (event->buttons().testFlag(Qt::MiddleButton))
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
