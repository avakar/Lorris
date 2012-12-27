#ifndef DIGITALTRACEGRAPH_H
#define DIGITALTRACEGRAPH_H

#include <QWidget>
#include <map>

class DigitalTraceGraph : public QWidget
{
    Q_OBJECT
public:
    struct block_info
    {
        size_t block_offset;
        size_t block_length;
        size_t repeat_count;

        block_info()
            : block_offset(0), block_length(0), repeat_count(0)
        {
        }

        block_info(size_t block_offset, size_t block_length, size_t repeat_count)
            : block_offset(block_offset), block_length(block_length), repeat_count(repeat_count)
        {
        }
    };

    struct channel_data
    {
        std::map<size_t, block_info> blocks;
        std::vector<bool> data; // yes, I do mean `vector<bool>`

        size_t length() const;
        bool sample(size_t index) const;
        std::pair<size_t, size_t> multisample(size_t first, size_t last);
    };

    explicit DigitalTraceGraph(QWidget *parent = 0);
    void setChannelData(std::vector<channel_data> & data);

public slots:
    void zoomToAll();

protected:
    void paintEvent(QPaintEvent * event);
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

private:
    std::vector<channel_data> m_channels;

    double m_panx;
    double m_samplesPerPixel;

    QPoint m_dragBase;
};

#endif // DIGITALTRACEGRAPH_H
