#ifndef DIGITALTRACEGRAPH_H
#define DIGITALTRACEGRAPH_H

#include <QWidget>
#include <map>
#include <list>

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

    struct sample_ptr
    {
        std::map<size_t, block_info>::const_iterator block_it;
        size_t block_offset;
        size_t repeat_index;
        size_t repeat_offset;

        bool first_repeat;
        bool last_repeat;
    };

    // TODO: rename to trace
    struct channel_data
    {
        std::map<size_t, block_info> blocks;
        std::vector<bool> data; // yes, I do mean `vector<bool>`

        size_t length() const;
        bool sample(size_t index) const;
        std::pair<bool, bool> multisample(size_t first, size_t last);

        sample_ptr get_sample_ptr(size_t sample) const;
    };

    typedef channel_data trace;

    struct domain_mapping
    {
        std::list<trace>::iterator trace_ptr;
        double samples_per_second;
        double seconds_from_epoch;

        double start_time() const { return seconds_from_epoch; }
        double end_time() const { return seconds_from_epoch + trace_ptr->length() / samples_per_second; }
    };

    struct domain
    {
        std::vector<domain_mapping> trace_maps;

        void swap(domain & d)
        {
            trace_maps.swap(d.trace_maps);
        }
    };

    explicit DigitalTraceGraph(QWidget *parent = 0);
    void setChannelData(std::vector<channel_data> & data, double samples_per_second);

public slots:
    void zoomToAll();

protected:
    void paintEvent(QPaintEvent * event);
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

private:
    std::list<trace> m_traces;
    domain m_domain;

    double m_panx;
    double m_secondsPerPixel;

    QPoint m_dragBase;
};

#endif // DIGITALTRACEGRAPH_H
