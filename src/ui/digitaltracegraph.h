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

protected:
    void paintEvent(QPaintEvent * event);

private:
    std::vector<channel_data> m_channels;
};

#endif // DIGITALTRACEGRAPH_H
