#include "digitaltracegraph.h"
#include <QPaintEvent>
#include <QPainter>

size_t DigitalTraceGraph::channel_data::length() const
{
    if (blocks.empty())
        return 0;

    std::pair<size_t, block_info> const & bi = *blocks.rbegin();
    return bi.first + bi.second.block_length * bi.second.repeat_count;
}

bool DigitalTraceGraph::channel_data::sample(size_t index) const
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

std::pair<bool, bool> DigitalTraceGraph::channel_data::multisample(size_t first, size_t last)
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
            first_ptr.block_it->second.block_offset + first_ptr.repeat_offset);
        if (first_complete == last_complete)
            return res;
        --last_complete;
    }

    reduce_block(res, data, first_complete->second.block_offset, last_complete->second.block_offset + last_complete->second.block_length);
    return res;
}

DigitalTraceGraph::sample_ptr DigitalTraceGraph::channel_data::get_sample_ptr(size_t sample) const
{
    sample_ptr res;
    res.block_it = blocks.upper_bound(sample);
    Q_ASSERT(res.block_it != blocks.begin());
    --res.block_it;

    res.block_offset = sample - res.block_it->second.block_offset;
    res.repeat_index = res.block_offset / res.block_it->second.block_length;
    res.repeat_offset = res.block_offset % res.block_it->second.block_length;

    res.first_repeat = res.repeat_index == 0;
    res.last_repeat = res.repeat_index == res.block_it->second.repeat_count - 1;
    return res;
}

DigitalTraceGraph::DigitalTraceGraph(QWidget *parent)
    : QWidget(parent), m_panx(0), m_samplesPerPixel(1.0)
{
}

void DigitalTraceGraph::setChannelData(std::vector<channel_data> & data)
{
    m_channels.swap(data);
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
    for (size_t i = 0; i < m_channels.size(); ++i)
    {
        size_t channel_length = m_channels[i].length();
        int w = this->width();
        if (m_samplesPerPixel >= 1)
        {
            bool have_last_sample_pos = false;
            double last_sample_pos = 0;
            for (int x = 0; x < w; ++x)
            {
                double sample_pos = x*m_samplesPerPixel+m_panx;
                if (sample_pos < 0)
                    continue;
                if (sample_pos >= channel_length)
                    break;

                if (have_last_sample_pos)
                {
                    std::pair<bool, bool> ft = m_channels[i].multisample(last_sample_pos, sample_pos);
                    if (ft.first && ft.second)
                        p.drawLine(x, y + row_padding, x, y + row_height - row_padding);
                    else if (ft.first)
                        p.drawPoint(x, y + row_height - row_padding);
                    else if (ft.second)
                        p.drawPoint(x, y + row_padding);
                }
                last_sample_pos = sample_pos;
                have_last_sample_pos = true;
            }
        }
        else
        {
            bool have_last_point = false;
            QPoint last_point;
            for (int x = 0; x < w; ++x)
            {
                double sample_pos = x*m_samplesPerPixel+m_panx;
                if (sample_pos < 0 || sample_pos >= channel_length)
                    continue;

                bool sample = m_channels[i].sample((size_t)sample_pos);

                int yy = y + (sample? row_padding: row_height - row_padding);
                QPoint pt(x, yy);
                if (have_last_point)
                    p.drawLine(last_point, pt);
                last_point = pt;
                have_last_point = true;
            }
        }

        y += row_height;
    }
}

void DigitalTraceGraph::zoomToAll()
{
    size_t max_length = 0;
    for (size_t i = 0; i < m_channels.size(); ++i)
        max_length = (std::max)(max_length, m_channels[i].length());

    if (max_length == 0)
        return;

    m_samplesPerPixel = (double)max_length / this->width();
    m_panx = 0;
    this->update();
}

void DigitalTraceGraph::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
        m_dragBase = event->pos();
}

void DigitalTraceGraph::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons().testFlag(Qt::LeftButton))
    {
        QPoint delta = event->pos() - m_dragBase;
        m_panx -= delta.x() * m_samplesPerPixel;
        m_dragBase = event->pos();
        this->update();
    }
}

void DigitalTraceGraph::wheelEvent(QWheelEvent * event)
{
    int delta = event->delta();
    m_panx += event->x() * m_samplesPerPixel;
    m_samplesPerPixel *= pow(1.2, -delta/120.0);
    m_panx -= event->x() * m_samplesPerPixel;
    this->update();
}
