#include "omicronanalconn.h"
#include "../misc/utils.h"
#include <stdexcept>
#include <QEvent>
#include <QCoreApplication>

static yb::usb_control_code_t const cmd_set_wraddr = { 0x41, 0x01 };
static yb::usb_control_code_t const cmd_set_rdaddr = { 0x41, 0x02 };
static yb::usb_control_code_t const cmd_start = { 0x41, 0x03 };
static yb::usb_control_code_t const cmd_stop = { 0x41, 0x04 };
static yb::usb_control_code_t const cmd_get_sample_index = { 0xc1, 0x05 };
static yb::usb_control_code_t const cmd_get_config = { 0xc1, 0x06 };
static yb::usb_control_code_t const cmd_unchoke = { 0xc1, 0x07 };
static yb::usb_control_code_t const cmd_move_choke = { 0x41, 0x08 };

template <typename T>
static T load(QFile & file)
{
    T res;
    if (file.read((char *)&res, sizeof res) != sizeof res)
        throw 1;
    return res;
}

OmicronAnalyzerConnection::OmicronAnalyzerConnection(yb::async_runner & runner)
    : TracelyzerConnection(CONNECTION_OMICRON_ANALYZER), m_runner(runner)
{
    static char const * inputNames[] = {
        "ch0", "ch1", "ch2", "ch3",
        "ch4", "ch5", "ch6", "ch7",
        "ch8", "ch9", "ch10", "ch11",
        "ch12", "ch13", "ch14", "ch15",
        "usb_tx_se0", "usb_tx_j", "usb_tx_en",
        "usb_rx_se0", "usb_rx_j", "usb_pullup"
        "usb_dn", "usb_dp",
        "spi_miso", "spi_mosi", "spi_clk", "spi_cs",
        "clk_24", "clk_33"
    };

    for (signal_trace_set::channel_id_t ch_id = 0; ch_id < sizeof inputNames / sizeof inputNames[0]; ++ch_id)
        m_inputNames[ch_id] = inputNames[ch_id];

    std::fill(m_mux, m_mux + 16, 31);
    this->markMissing();
    connect(&m_notify_channel, SIGNAL(dataReceived()), this, SLOT(readChannelReceive()));
}

static QString fromUtf8(std::string const & s)
{
    return QString::fromUtf8(s.data(), s.size());
}

static QString formatDeviceDetails(yb::usb_device const & dev)
{
    return QString("SN %1").arg(fromUtf8(dev.serial_number()));
}

QString OmicronAnalyzerConnection::details() const
{
    return m_details;
}

std::map<signal_trace_set::channel_id_t, std::string> const & OmicronAnalyzerConnection::inputNames() const
{
    return m_inputNames;
}

void OmicronAnalyzerConnection::setup(yb::usb_device_interface const & intf, ShupitoDesc const & ybdesc)
{
    assert(!intf.empty());
    yb::usb_interface const & idesc = intf.descriptor();
    assert(idesc.altsettings.size() == 1); // XXX

    yb::usb_interface_descriptor const & desc = idesc.altsettings[0];

    m_intf = intf;

    assert(desc.endpoints.size() == 3);
    m_out_ep = desc.endpoints[1].bEndpointAddress;
    m_out_ep_size = desc.endpoints[1].wMaxPacketSize;
    m_in_ep = desc.endpoints[0].bEndpointAddress;
    m_notify_ep = desc.endpoints[2].bEndpointAddress;

    m_details = formatDeviceDetails(intf.device());

    m_desc = ybdesc;
    this->markPresent();
}

void OmicronAnalyzerConnection::clear()
{
    this->closeImpl();
    m_intf.clear();
    this->markMissing();
}

void OmicronAnalyzerConnection::closeImpl()
{
    m_intf_guard.release();
}

void OmicronAnalyzerConnection::doOpen()
{
    yb::usb_device dev = m_intf.device();
    if (!m_intf_guard.claim(dev, m_intf.interface_index()))
        return Utils::showErrorBox("Cannot claim the interface");

    this->SetState(st_connected);
}

void OmicronAnalyzerConnection::doClose()
{
    emit disconnecting();
    this->closeImpl();
    this->SetState(st_disconnected);
}

int OmicronAnalyzerConnection::vid() const
{
    if(m_intf.empty())
        return 0;
    return m_intf.device().vidpid() >> 16;
}

int OmicronAnalyzerConnection::pid() const
{
    if(m_intf.empty())
        return 0;
    return m_intf.device().vidpid() & 0xFFFF;
}

QString OmicronAnalyzerConnection::serialNumber() const
{
    if(m_intf.empty())
        return QString();
    return QString::fromUtf8(m_intf.device().serial_number().c_str());
}

QString OmicronAnalyzerConnection::intfName() const
{
    if(m_intf.empty())
        return QString();

    QString name = QString::fromUtf8(m_intf.name().c_str());
    if (name.isEmpty())
        name = QString("#%1").arg(m_intf.interface_index());
    return name;
}

QHash<QString, QVariant> OmicronAnalyzerConnection::config() const
{
    QHash<QString, QVariant> cfg = Connection::config();
    cfg["vid"] = this->vid();
    cfg["pid"] = this->pid();
    cfg["serial_number"] = this->serialNumber();
    cfg["intf_name"] = this->intfName();
    return cfg;
}

size_t OmicronAnalyzerConnection::maxChannelCount() const
{
    return 16;
}

void OmicronAnalyzerConnection::setChannel(size_t channel, size_t input)
{
    assert(channel < 16);
    m_mux[channel] = (std::min)(input, (size_t)31);
}

void OmicronAnalyzerConnection::setChannelCount(size_t channels)
{
    for (size_t ch = channels; ch < 16; ++ch)
        m_mux[ch] = 31;
}

double OmicronAnalyzerConnection::maxFrequency() const
{
    return 100000000;
}

template <typename T>
static uint8_t * store_le(uint8_t * p, T value, size_t size = sizeof(T))
{
    while (size)
    {
        *p++ = value;
        value >>= 8;
        --size;
    }

    return p;
}

template <typename T>
static T load_le(uint8_t const * p, size_t size = sizeof(T))
{
    T res = 0;
    for (size_t i = size; i != 0; --i)
        res = (res << 8) | p[i-1];
    return res;
}

void OmicronAnalyzerConnection::startTrace(signal_trace_set & output, double freq)
{
    int channel_count = 16;
    while (channel_count && m_mux[channel_count-1] == 31)
        --channel_count;

    if (!channel_count)
        return;

    uint32_t mux1 = m_mux[0]
            | (m_mux[1] << 5)
            | (m_mux[2] << 10)
            | (m_mux[3] << 15)
            | (m_mux[4] << 20)
            | (m_mux[5] << 25)
            | (m_mux[6] << 30);
    uint32_t mux2 = m_mux[6] >> 2
            | (m_mux[7] << 3)
            | (m_mux[8] << 8)
            | (m_mux[9] << 13)
            | (m_mux[10] << 18)
            | (m_mux[11] << 23)
            | (m_mux[12] << 28);
    uint32_t mux3 = m_mux[13] >> 4
            | (m_mux[14] << 1)
            | (m_mux[15] << 6)
            | (31 << 11);

    int log_channels = 0;
    {
        int ch = 1;
        while (ch < channel_count)
        {
            ++log_channels;
            ch *= 2;
        }
        m_rounded_channel_count = ch;
    }

    uint8_t packet[18] = {};
    packet[0] = log_channels;
    packet[1] = 0;
    store_le<uint32_t>(packet + 2, (uint32_t)(100000000 / freq) - 1);
    store_le(packet + 6, mux1);
    store_le(packet + 10, mux2);
    store_le(packet + 14, mux3);
    m_runner.run(m_intf.device().control_write(cmd_start, 0, m_intf.interface_index(), packet, sizeof packet));
    m_channel_count = channel_count;
    m_samples_per_second = freq;

    m_open_traces.clear();

    m_trace_set = &output;

    m_choked = true;
    m_read_loop = m_runner.post(this->readContinuously());
}

yb::task<void> OmicronAnalyzerConnection::readMem()
{
    store_le<uint32_t>(m_buffer, m_start_addr & ~31);
    return m_intf.device().control_write(cmd_set_rdaddr, 0, m_intf.interface_index(), m_buffer, 4).then([this](){
        return yb::loop([this](yb::cancel_level cl) -> yb::task<void> {
            if (cl >= yb::cl_quit)
                return yb::nulltask;

            if (m_start_addr == m_end_addr)
                return yb::nulltask;

            uint32_t total_len = (m_end_addr - (m_start_addr & ~31)) * 2;
            total_len = (total_len + 63) & ~63;
            total_len &= 0x1ffffff;
            uint32_t len = (std::min)(total_len, sizeof m_buffer);
            assert((len % 64) == 0);
            return m_intf.device().bulk_read(m_in_ep, m_buffer, len).then([this](size_t r){
                uint32_t aligned_addr = (m_start_addr & ~31);
                uint32_t total_len = (m_end_addr - aligned_addr) * 2;
                total_len &= 0x1ffffff;
                uint32_t len = (std::min)(total_len, r);
                {
                    QMutexLocker l(&m_mutex);

                    if (m_data.empty() || m_data.back().first != m_start_index)
                        m_data.push_back(std::make_pair(m_start_index, std::vector<uint8_t>()));

                    std::vector<uint8_t> & data = m_data.back().second;
                    data.insert(data.end(), m_buffer + 2*(m_start_addr & 31), m_buffer + len);
                    m_notify_channel.send();
                }
                m_start_addr = (aligned_addr + len / 2) & 0xffffff;
            });
        });
    });
}

yb::task<void> OmicronAnalyzerConnection::readNext()
{
    return m_intf.device().control_read(cmd_get_sample_index, 0, m_intf.interface_index(), m_buffer, 64).then([this](size_t r){
        m_choked = (r == 12);

        m_end_addr = load_le<uint32_t>(m_buffer);
        return this->readMem().then([this](){
            store_le<uint32_t>(m_buffer, m_end_addr);
            return m_intf.device().control_write(cmd_move_choke, 0, m_intf.interface_index(), m_buffer, 4);
        });
    });
}

yb::task<void> OmicronAnalyzerConnection::readContinuously()
{
    return yb::loop([this](yb::cancel_level cl) -> yb::task<void> {
        if (cl >= yb::cl_quit)
            return yb::nulltask;

        if (m_choked)
        {
            return m_intf.device().control_read(cmd_unchoke, 0, m_intf.interface_index(), m_buffer, 64).then([this](size_t) {
                m_start_addr = load_le<uint32_t>(m_buffer);
                m_start_index = load_le<uint64_t>(m_buffer + 4);

                return this->readNext();
            });
        }
        else
        {
            return this->readNext();
        }
    });
}

void OmicronAnalyzerConnection::stopTrace()
{
    m_read_loop.wait(yb::cl_quit);

    m_runner.run(m_intf.device().control_write(cmd_stop, 0, m_intf.interface_index(), 0, 0));
}

template <typename T>
static void store(QFile & file, T t)
{
    file.write((char const *)&t, sizeof t);
}

void OmicronAnalyzerConnection::readChannelReceive()
{
    std::vector<std::pair<uint64_t, std::vector<uint8_t> > > data;

    {
        QMutexLocker l(&m_mutex);
        data.swap(m_data);
    }

    this->processData(data);
}

#ifdef VERIFY_TRACE_SETS
static bool verifyTraceSet(signal_trace_set const & data)
{
    for (auto && kv: data.traces)
    {
        size_t cur_sample_index = 0;
        size_t cur_block_offset = 0;
        for (auto && block: kv.second.blocks)
        {
            if (block.first != cur_sample_index)
                return false;

            if (block.second.block_offset != cur_block_offset)
                return false;

            if (block.second.repeat_count == 0)
                return false;

            cur_sample_index += block.second.block_length * block.second.repeat_count;
            cur_block_offset += block.second.block_length;
        }

        if (cur_block_offset != kv.second.data.size())
            return false;
    }

    return true;
}
#endif

void OmicronAnalyzerConnection::storeSample(uint16_t sample, signal_trace::block_info & bi)
{
    uint16_t tmp2 = sample;
    size_t bits_remaining = 16;
    while (bits_remaining)
    {
        uint16_t tmp = tmp2 >> (16 - m_rounded_channel_count);
        tmp2 <<= m_rounded_channel_count;
        bits_remaining -= m_rounded_channel_count;

        for (size_t i = 0; i < m_rounded_channel_count; ++i)
        {
            if (i < m_open_traces.size())
                m_open_traces[i]->data.push_back((tmp & 1) == 0);
            tmp >>= 1;
        }
    }

    bi.block_length += 16 / m_rounded_channel_count;
}

void OmicronAnalyzerConnection::storeBi(signal_trace::block_info & bi)
{
    assert(bi.repeat_count != 0);
    assert(bi.block_length != 0);

    for (size_t i = 0; i < m_open_traces.size(); ++i)
    {
        assert(m_open_traces[i]->blocks.find(m_sample_index) == m_open_traces[i]->blocks.end());
        m_open_traces[i]->blocks[m_sample_index] = bi;
    }
    m_sample_index += bi.block_length * bi.repeat_count;
    bi.block_offset += bi.block_length;
    bi.block_length = 0;
    bi.repeat_count = 0;
}

void OmicronAnalyzerConnection::popSample(signal_trace::block_info & bi)
{
    for (size_t i = 0; i < m_open_traces.size(); ++i)
    {
        auto & data = m_open_traces[i]->data;
        data.erase(data.begin() + (data.size() - 16 / m_rounded_channel_count), data.end());
    }

    bi.block_length -= 16 / m_rounded_channel_count;
}

void OmicronAnalyzerConnection::processData(std::vector<std::pair<uint64_t, std::vector<uint8_t> > > const & data)
{
    for (std::pair<uint64_t, std::vector<uint8_t> > const & inc_trace: data)
    {
        if (inc_trace.second.size() == 0)
            continue;

        signal_trace::block_info bi;
        bi.block_length = 0;

        if (m_open_traces.empty() || m_open_trace_marker != inc_trace.first)
        {
            m_open_traces.clear();
            m_open_trace_marker = inc_trace.first;

            m_compress_sample = m_open_trace_marker >> 48;
            m_compress_state = (compress_state_t)((m_open_trace_marker >> 46) & 0x3);
            m_sample_index = 0;

            for (size_t i = 0; i < m_channel_count; ++i)
            {
                auto it = m_trace_set->traces.insert(std::make_pair(m_mux[i], signal_trace()));
                m_open_traces.push_back(&it->second);
                it->second.samples_per_second = m_samples_per_second;
                it->second.samples_from_epoch = (m_open_trace_marker & 0x3fffffffffff) * (16 / m_rounded_channel_count);
            }

            bi.block_offset = 0;
        }
        else
        {
            assert(!m_open_traces.front()->blocks.empty());
            signal_trace::block_info const & last_bi = std::prev(m_open_traces.front()->blocks.end())->second;
            bi.block_offset = last_bi.block_offset + last_bi.block_length;
        }

        uint8_t const * first = inc_trace.second.data();
        uint8_t const * last = first + inc_trace.second.size();

        if (m_compress_state == st_count)
        {
            this->storeSample(m_compress_sample, bi);
            bi.repeat_count = 0;
        }
        else
        {
            bi.repeat_count = 1;
        }

        for (uint8_t const * cur = first; cur != last;)
        {
            uint16_t sample;
            sample = (*cur++);
            sample |= (*cur++) << 8;

            switch (m_compress_state)
            {
            case st_prefirst:
                this->storeSample(sample, bi);
                m_compress_state = st_idle;
                break;
            case st_idle:
                if (m_compress_sample == sample)
                {
                    if (bi.block_length != 0)
                    {
                        this->popSample(bi);
                        if (bi.block_length != 0)
                        {
                            assert(bi.repeat_count == 1);
                            this->storeBi(bi);
                        }
                    }

                    this->storeSample(sample, bi);
                    bi.repeat_count = 2;
                    m_compress_state = st_count;
                }
                else
                {
                    this->storeSample(sample, bi);
                }
                break;
            case st_count:
                bi.repeat_count += sample;
                if (sample != 0xffff)
                {
                    if (bi.repeat_count == 0)
                    {
                        this->popSample(bi);
                        assert(bi.block_length == 0);
                    }
                    else
                    {
                        this->storeBi(bi);
                    }
                    bi.repeat_count = 1;
                    m_compress_state = st_prefirst;
                }
                break;
            }

            m_compress_sample = sample;
        }

        if (bi.block_length != 0)
            this->storeBi(bi);
    }

#ifdef VERIFY_TRACE_SETS
    if (!verifyTraceSet(m_trace_set))
        throw QString("verifyTraceSet failed");
#endif
    emit onData();
}

std::vector<signal_trace_set::channel_id_t> OmicronAnalyzerConnection::defaultInputs() const
{
    std::vector<signal_trace_set::channel_id_t> res;
    //for (signal_trace_set::channel_id_t id = 0; id < 16; ++id)
    //    res.push_back(id);

    // XXX
    res.push_back(20);
    res.push_back(19);

    return res;
}
