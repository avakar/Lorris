#include "lorrisomicron.h"
#include "ui_lorrisomicron.h"
#include <utility>

LorrisOmicron::LorrisOmicron()
    : ui(new Ui::LorrisOmicron), m_run_state(st_stopped)
{
    ui->setupUi(this);

    m_connectButton = new ConnectButton(ui->connectButton);
    m_connectButton->setConnectionTypes(pct_shupito);
    connect(m_connectButton, SIGNAL(connectionChosen(ConnectionPointer<Connection>)), this, SLOT(setConnection(ConnectionPointer<Connection>)));

    ui->readoutContainer->setVisible(false);
    connect(ui->startStopButton, SIGNAL(clicked()), this, SLOT(startStoppedClicked()));

    this->updateUi();
}

LorrisOmicron::~LorrisOmicron()
{
    delete ui;
}

QString LorrisOmicron::GetIdString()
{
    return "LorrisOmicron";
}

void LorrisOmicron::setConnection(ConnectionPointer<Connection> const & conn)
{
    if (m_conn)
    {
        m_conn->disconnect(this);
    }

    m_conn.reset();
    m_run_state = st_stopped;

    if (ConnectionPointer<ShupitoConnection> c = conn.dynamicCast<ShupitoConnection>())
    {
        m_conn = c;
        c->setSupportsDescriptor(false);
        connect(c.data(), SIGNAL(packetRead(ShupitoPacket)), this, SLOT(packetRead(ShupitoPacket)));
        connect(c.data(), SIGNAL(stateChanged(ConnectionState)), this, SLOT(updateUi()));
    }

    this->updateUi();
}

template <typename T>
static T deserialize(uint8_t const * first, uint8_t const * last)
{
    T res = 0;
    while (first != last)
    {
        res <<= 8;
        res |= *--last;
    }
    return res;
}

template <typename T>
static T deserialize(uint8_t const * first)
{
    return deserialize<T>(first, first + sizeof(T));
}

void LorrisOmicron::readNextChunk()
{
    if (m_start_addr == m_stop_addr)
    {
        this->setRunState(st_stopped);
        this->handle_captured_data();
        return;
    }

    uint16_t chunk_len = (std::min)(m_stop_addr - m_start_addr, (uint32_t)0xfff);
    m_requested_length = 2*chunk_len;
    this->readMem(m_start_addr, chunk_len);
    m_start_addr += chunk_len;
}

void LorrisOmicron::packetRead(ShupitoPacket const & p)
{
    if (p[0] == 1)
    {
        m_captured_data.insert(m_captured_data.end(), p.begin() + 1, p.end());
        ui->readoutProgress->setValue(m_start_addr - m_requested_length);
        if (m_requested_length < p.size() - 1)
        {
            m_requested_length = 0;
            // XXX: report error
            this->setRunState(st_stopped);
        }
        else
        {
            m_requested_length -= p.size() - 1;
            if (m_requested_length == 0)
                this->readNextChunk();
        }
    }
    else if (p[0] == 3 && p.size() == 0xe)
    {
        uint32_t sample_index = deserialize<uint32_t>(&p[1]);
        uint32_t trigger_timestamp = deserialize<uint32_t>(&p[5]);
        uint32_t stop_addr = deserialize<uint32_t>(&p[9]);
        uint8_t flags = p[13];

        m_stop_index = sample_index;
        m_stop_addr = stop_addr;

        ui->readoutProgress->setRange(m_start_addr, m_stop_addr);
        ui->readoutProgress->setValue(m_start_addr);
        this->setRunState(st_reading_samples);

        m_captured_data.clear();
        this->readNextChunk();
    }
    else if (p[0] == 2 && p.size() == 9)
    {
        uint32_t sample_index = deserialize<uint32_t>(&p[1]);
        uint32_t start_addr = deserialize<uint32_t>(&p[5]);

        m_start_index = sample_index;
        m_start_addr = start_addr;
        this->setRunState(st_running);
    }
}

void LorrisOmicron::startStoppedClicked()
{
    Q_ASSERT(m_conn.data());

    switch (m_run_state)
    {
    case st_stopped:
        {
            uint16_t rising_mask = 0;
            uint16_t falling_mask = 0;
            uint32_t period = 100;
            uint8_t log_channels = 4;

            ShupitoPacket p = makeShupitoPacket(2, 9,
                rising_mask, rising_mask >> 8,
                falling_mask, falling_mask >> 8,
                period, period >> 8, period >> 16, period >> 24,
                log_channels);
            m_conn->sendPacket(p);
            this->setRunState(st_start_requested);
        }
        break;
    default:
        m_conn->sendPacket(makeShupitoPacket(3, 0));
        this->setRunState(st_stop_requested);
        break;
    }
}

void LorrisOmicron::setRunState(run_state_t state)
{
    m_run_state = state;
    switch (state)
    {
    case st_stopped:
        ui->startStopButton->setText(tr("Start"));
        break;
    case st_start_requested:
        ui->startStopButton->setText(tr("Starting"));
        break;
    case st_running:
        ui->startStopButton->setText(tr("Stop"));
        break;
    case st_stop_requested:
    case st_reading_samples:
        ui->startStopButton->setText(tr("Stopping"));
        break;
    }

    ui->startStopButton->setEnabled(state != st_reading_samples);
    ui->readoutContainer->setVisible(state == st_reading_samples);
}

void LorrisOmicron::readMem(uint32_t addr, uint16_t len)
{
    Q_ASSERT(m_conn.data());

    ShupitoPacket p = makeShupitoPacket(1, 6,
        addr, addr >> 8, addr >> 16, addr >> 24,
        len, len >> 8);
    m_conn->sendPacket(p);
}

void LorrisOmicron::handle_captured_data()
{
    uint8_t const * first = m_captured_data.data();
    uint8_t const * last = first + m_captured_data.size();

    if ((last - first) % 2 != 0)
        return;

    std::vector<DigitalTraceGraph::channel_data> channels;
    channels.resize(16);

    DigitalTraceGraph::block_info bi = {};
    bi.repeat_count = 1;

    enum { st_prefirst, st_idle, st_count } state = st_prefirst;
    size_t sample_index = 0;
    uint16_t prev_sample = 0;
    bool done = false;
    for (uint8_t const * cur = first; !done;)
    {
        uint16_t sample;

        if (cur != last)
        {
            sample = (*cur++) << 8;
            sample |= *cur++;
        }
        else if (state == st_count)
        {
            break;
        }
        else
        {
            done = true;
            sample = prev_sample + 1;
        }

        switch (state)
        {
        case st_prefirst:
            state = st_idle;
            break;
        case st_idle:
            if (prev_sample == sample)
            {
                if (bi.block_length != 0)
                {
                    for (size_t i = 0; i < channels.size(); ++i)
                        channels[i].blocks[sample_index] = bi;
                    sample_index += bi.block_length;
                    bi.block_offset += bi.block_length;
                }

                uint16_t tmp = prev_sample;
                for (size_t i = 0; i < channels.size(); ++i)
                {
                    channels[i].data.push_back((tmp & 1) != 0);
                    tmp >>= 1;
                }

                bi.block_length = 1;
                bi.repeat_count = 2;
                state = st_count;
            }
            else
            {
                uint16_t tmp = prev_sample;
                for (size_t i = 0; i < channels.size(); ++i)
                {
                    channels[i].data.push_back((tmp & 1) != 0);
                    tmp >>= 1;
                }
                ++bi.block_length;
            }
            break;
        case st_count:
            Q_ASSERT(bi.block_length == 1);
            bi.repeat_count += sample;
            if (sample != 0xffff)
            {
                for (size_t i = 0; i < channels.size(); ++i)
                    channels[i].blocks[sample_index] = bi;
                sample_index += bi.repeat_count;
                bi.repeat_count = 1;
                bi.block_offset += bi.block_length;
                bi.block_length = 0;
                state = st_prefirst;
            }
            break;
        }

        prev_sample = sample;
    }

    ui->graph->setChannelData(channels);
}

void LorrisOmicron::updateUi()
{
    ui->startStopButton->setEnabled(m_conn && m_conn->state() == st_connected);
}
