#ifndef DIGITALTRACEGRAPH_H
#define DIGITALTRACEGRAPH_H

#include <QWidget>
#include <map>
#include <list>

class DigitalTraceGraph : public QWidget
{
    Q_OBJECT
public:
    enum interpolation_t
    {
        i_point,
        i_linear
    };

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

    struct sample_ptr
    {
        std::map<size_t, block_info>::const_iterator block_it;
        size_t data_offset;
        size_t block_sample_offset;
        size_t repeat_index;
        size_t repeat_offset;

        bool first_repeat;
        bool last_repeat;
    };

    struct trace_t
    {
        std::map<size_t, block_info> blocks;
        std::vector<bool> data; // yes, I do mean `vector<bool>`

        double samples_per_second;
        double seconds_from_epoch;

        size_t length() const;
        bool sample(size_t index) const;
        std::pair<bool, bool> multisample(size_t first, size_t last) const;

        sample_ptr get_sample_ptr(size_t sample) const;

        double start_time() const { return seconds_from_epoch; }
        double end_time() const { return seconds_from_epoch + this->length() / samples_per_second; }
    };

    struct domain
    {
        std::vector<trace_t> traces;

        void swap(domain & d)
        {
            traces.swap(d.traces);
        }
    };

    explicit DigitalTraceGraph(QWidget *parent = 0);
    void setChannelData(std::vector<trace_t> & data);

    interpolation_t interpolation() const;
    void setInterpolation(interpolation_t v);

public slots:
    void zoomToAll();

protected:
    void paintEvent(QPaintEvent * event);
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

private:
    domain m_domain;
    double m_panx;
    double m_secondsPerPixel;

    interpolation_t m_interpolation;

    QPoint m_dragBase;
    QPoint m_lastCursorPos;
};

#endif // DIGITALTRACEGRAPH_H
