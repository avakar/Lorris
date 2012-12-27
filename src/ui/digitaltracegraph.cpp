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

std::pair<size_t, size_t> DigitalTraceGraph::channel_data::multisample(size_t first, size_t last)
{
    std::map<size_t, block_info>::const_iterator it = blocks.upper_bound(first);
    Q_ASSERT(it != blocks.begin());
    --it;

    size_t f_count = 0;
    size_t t_count = 0;

    /*size_t block_offset = index - it->first;
    size_t repeat_index = block_offset / it->second.block_length;

    for (size_t i = block_offset % it->second.block_length; i < it->second.block_length; ++i)
    {
        if (it->data[i])
            ++t_count;
        else
            ++f_count;
    }

    ++repeat_index;
    for (; it != blocks.end(); ++it)
    {
        if (repeat_index == it->second.repeat_index)
        {
            repeat_index = 0;
            continue;
        }

        size_t f = 0, t = 0;
        for (size_t i = 0; i < it->second.block_length; ++i)
        {
            if (data[it->first + i])
                ++t;
            else
                ++f;
        }

        t *= it->second.repeat_ciount - repeat_index;
        f *= it->second.repeat_count - repeat_index;

        t_count += t;
        f_count += f;
        repeat_index = 0;
    }

    for (; repeat_index < it->second.repeat_count; ++repeat_index)
    {
    }*/

    return std::make_pair(f_count, t_count);
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
