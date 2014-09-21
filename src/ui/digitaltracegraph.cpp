#include "digitaltracegraph.h"
#include <QPaintEvent>
#include <QPainter>

static size_t clamp(double v)
{
    if (v < 0)
        return 0;
    if (v > std::numeric_limits<size_t>::max())
        return std::numeric_limits<size_t>::max();
    return (size_t)v;
}

size_t DigitalTraceGraph::trace_t::length() const
{
    if (blocks.empty())
        return 0;

    std::pair<size_t, block_info> const & bi = *blocks.rbegin();
    return bi.first + bi.second.block_length * bi.second.repeat_count;
}

bool DigitalTraceGraph::trace_t::sample(size_t index) const
{
    std::map<size_t, block_info>::const_iterator it = blocks.upper_bound(index);
    Q_ASSERT(it != blocks.begin());
    --it;

    index -= it->first;
    index %= it->second.block_length;

    return data[it->second.block_offset + index];
}

static void reduce_block(std::pair<bool, bool> & res, std::vector<bool> const & v, size_t first, size_t last)
{
    for (; (!res.first || !res.second) && first != last; ++first)
    {
        if (v[first])
            res.second = true;
        else
            res.first = true;
    }
}

std::pair<bool, bool> DigitalTraceGraph::trace_t::multisample(size_t first, size_t last) const
{
    sample_ptr first_ptr = this->get_sample_ptr(first);
    sample_ptr last_ptr = this->get_sample_ptr(last);

    std::pair<bool, bool> res(false, false);

    if (first_ptr.block_it == last_ptr.block_it
        && first_ptr.repeat_index == last_ptr.repeat_index)
    {
        reduce_block(res, data,
            first_ptr.block_it->second.block_offset + first_ptr.repeat_offset,
            last_ptr.block_it->second.block_offset + last_ptr.repeat_offset);
        return res;
    }

    std::map<size_t, block_info>::const_iterator first_complete = first_ptr.block_it;
    std::map<size_t, block_info>::const_iterator last_complete = last_ptr.block_it;
    if (first_ptr.last_repeat)
    {
        // Handle partial block
        reduce_block(res, data, first_ptr.block_it->second.block_offset + first_ptr.repeat_offset,
            first_ptr.block_it->second.block_offset + first_ptr.block_it->second.block_length);
        ++first_complete;
    }

    if (last_ptr.first_repeat)
    {
        reduce_block(res, data, last_ptr.block_it->second.block_offset,
            last_ptr.block_it->second.block_offset + last_ptr.repeat_offset);
        if (first_complete == last_complete)
            return res;
        --last_complete;
    }

    reduce_block(res, data, first_complete->second.block_offset, last_complete->second.block_offset + last_complete->second.block_length);
    return res;
}

DigitalTraceGraph::sample_ptr DigitalTraceGraph::trace_t::get_sample_ptr(size_t sample) const
{
    sample_ptr res;
    res.block_it = blocks.upper_bound(sample);
    Q_ASSERT(res.block_it != blocks.begin());
    --res.block_it;

    res.block_sample_offset = sample - res.block_it->first;
    res.repeat_index = res.block_sample_offset / res.block_it->second.block_length;
    res.repeat_offset = res.block_sample_offset % res.block_it->second.block_length;
    res.data_offset = res.block_it->second.block_offset + res.repeat_offset;

    res.first_repeat = res.repeat_index == 0;
    res.last_repeat = res.repeat_index == res.block_it->second.repeat_count - 1;
    return res;
}

DigitalTraceGraph::DigitalTraceGraph(QWidget *parent)
    : QWidget(parent), m_panx(0), m_secondsPerPixel(1.0), m_interpolation(i_point)
{
    this->setMouseTracking(true);
}

void DigitalTraceGraph::setChannelData(std::vector<trace_t> & data)
{
    m_domain.traces.swap(data);
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

    domain * const selected_domain = &m_domain;
    for (size_t i = 0; i < selected_domain->traces.size(); ++i)
    {
        trace_t const & t = selected_domain->traces[i];

        size_t channel_length = t.length();
        int w = this->width();
        if (m_secondsPerPixel*t.samples_per_second >= 1)
        {
            bool have_last_sample_pos = false, have_last_ft = false;
            double last_sample_pos = 0;
            std::pair<bool, bool> last_ft;
            for (int x = 0; x < w; ++x)
            {
                double sample_pos = (x*m_secondsPerPixel+m_panx)*t.samples_per_second;
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
            size_t first_sample = clamp(m_panx*t.samples_per_second);
            size_t last_sample = clamp((w*m_secondsPerPixel+m_panx)*t.samples_per_second + 2);

            QPoint last_point;
            double last_x;
            for (size_t cur = first_sample; cur != last_sample; ++cur)
            {
                bool sample = t.sample(cur);

                double x = (cur/t.samples_per_second-m_panx)/m_secondsPerPixel;
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

        y += row_height;
    }

    p.drawText(2, this->height() - 10, QString("samples/s: %1, seconds/px: %2, panx: %4, x: %3").arg(selected_domain->traces[0].samples_per_second, 0, 'f').arg(m_secondsPerPixel, 0, 'f', 15).arg(m_lastCursorPos.x()).arg(m_panx));
}

void DigitalTraceGraph::zoomToAll()
{
    double min_time, max_time;

    domain * const selected_domain = &m_domain;
    if (selected_domain->traces.empty())
        return;

    for (size_t i = 0; i < selected_domain->traces.size(); ++i)
    {
        trace_t const & m = selected_domain->traces[i];
        double start_time = m.start_time();
        double end_time = m.end_time();

        if (i == 0)
        {
            min_time = start_time;
            max_time = end_time;
        }
        else
        {
            min_time = (std::min)(min_time, start_time);
            max_time = (std::max)(max_time, start_time);
        }
    }

    m_secondsPerPixel = (max_time - min_time) / this->width();
    m_panx = min_time;
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
    int delta = event->delta();
    m_panx += event->x() * m_secondsPerPixel;
    m_secondsPerPixel *= pow(1.2, -delta/120.0);
    m_panx -= event->x() * m_secondsPerPixel;
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
