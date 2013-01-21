#include "lorrisomicron.h"
#include "ui_lorrisomicron.h"
#include <utility>
#include <QFileDialog>

LorrisOmicron::LorrisOmicron()
    : ui(new Ui::LorrisOmicron), m_run_state(st_stopped)
{
    ui->setupUi(this);

    QCheckBox * const enableBoxes[enableBoxCount] = {
        ui->ch0enableBox,
        ui->ch1enableBox,
        ui->ch2enableBox,
        ui->ch3enableBox,
        ui->ch4enableBox,
        ui->ch5enableBox,
        ui->ch6enableBox,
        ui->ch7enableBox,
        ui->ch8enableBox,
        ui->ch9enableBox,
        ui->ch10enableBox,
        ui->ch11enableBox,
        ui->ch12enableBox,
        ui->ch13enableBox,
        ui->ch14enableBox,
        ui->ch15enableBox,
    };
    std::copy(enableBoxes, enableBoxes + enableBoxCount, m_enableBoxes);

    m_connectButton = new ConnectButton(ui->connectButton);
    m_connectButton->setConnectionTypes(pct_port_programmable);
    connect(m_connectButton, SIGNAL(connectionChosen(ConnectionPointer<Connection>)), this, SLOT(setConnection(ConnectionPointer<Connection>)));

    connect(ui->zoomAllButton, SIGNAL(clicked()), ui->graph, SLOT(zoomToAll()));

    ui->readoutContainer->setVisible(false);
    connect(ui->startStopButton, SIGNAL(clicked()), this, SLOT(startStoppedClicked()));

    QActionGroup * interpolationGroup = new QActionGroup(this);
    interpolationGroup->addAction(ui->actionInterpolationPoint);
    interpolationGroup->addAction(ui->actionInterpolationLinear);
    switch (sConfig.get(CFG_QUINT32_OMICRON_INTERPOLATION))
    {
    case 0:
        ui->actionInterpolationPoint->setChecked(true);
        this->interpolationGroupTriggered(ui->actionInterpolationPoint);
        break;
    case 1:
        ui->actionInterpolationLinear->setChecked(true);
        this->interpolationGroupTriggered(ui->actionInterpolationLinear);
        break;
    }
    connect(interpolationGroup, SIGNAL(triggered(QAction*)), this, SLOT(interpolationGroupTriggered(QAction*)));

    QMenu * interpolationMenu = new QMenu(tr("Interpolation"), this);
    interpolationMenu->addAction(ui->actionInterpolationPoint);
    interpolationMenu->addAction(ui->actionInterpolationLinear);
    this->addTopMenu(interpolationMenu);

    this->addTopAction(ui->actionImportTraces);
    this->addTopAction(ui->actionExportTraces);

    this->updateUi();

    this->importTraces("../etc/samples/pdi_init.omtr");
    ui->graph->zoomToAll();
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
        this->handle_captured_data(100000000 / m_period);
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
            uint32_t period = 100000000 / ui->sampleFreqSpin->value();
            uint8_t log_channels = 0;

            size_t highestEnabled = 0;
            for (size_t i = enableBoxCount; i != 0; --i)
            {
                if (m_enableBoxes[i-1]->isChecked())
                {
                    highestEnabled = i-1;
                    break;
                }
            }

            while (highestEnabled)
            {
                highestEnabled /= 2;
                ++log_channels;
            }

            ShupitoPacket p = makeShupitoPacket(2, 9,
                rising_mask, rising_mask >> 8,
                falling_mask, falling_mask >> 8,
                period, period >> 8, period >> 16, period >> 24,
                log_channels);
            m_conn->sendPacket(p);

            m_period = period;
            m_log_channels = log_channels;
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
    this->updateUi();
}

void LorrisOmicron::readMem(uint32_t addr, uint16_t len)
{
    Q_ASSERT(m_conn.data());

    ShupitoPacket p = makeShupitoPacket(1, 6,
        addr, addr >> 8, addr >> 16, addr >> 24,
        len, len >> 8);
    m_conn->sendPacket(p);
}

void LorrisOmicron::handle_captured_data(double samples_per_second)
{
    // workaround: sometimes, the first sample gets lost
    if (m_captured_data.size() > 2 && m_captured_data[2] == 0xff && m_captured_data[3] == 0xff)
    {
        uint8_t first_sample[2] = {
            m_captured_data[0],
            m_captured_data[1],
        };
        m_captured_data.insert(m_captured_data.begin(), first_sample, first_sample + sizeof first_sample);
    }

    uint8_t const * first = m_captured_data.data();
    uint8_t const * last = first + m_captured_data.size();

    if ((last - first) % 2 != 0)
        return;

    std::vector<DigitalTraceGraph::trace_t> channels;
    channels.resize(16);

    DigitalTraceGraph::block_info bi;
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

    std::vector<DigitalTraceGraph::trace_t> filtered_channels;
    for (size_t i = 0; i < enableBoxCount; ++i)
    {
        if (m_enableBoxes[i]->isChecked())
        {
            channels[i].samples_per_second = samples_per_second;
            channels[i].seconds_from_epoch = 0;
            filtered_channels.push_back(channels[i]);
        }
    }

    ui->graph->setChannelData(filtered_channels);
    ui->graph->zoomToAll();
}

void LorrisOmicron::updateUi()
{
    bool connected = m_conn && m_conn->state() == st_connected;
    ui->startStopButton->setEnabled(connected);
    ui->settingsContainer->setEnabled(!connected || m_run_state == st_stopped);
}

void LorrisOmicron::on_actionExportTraces_triggered()
{
    QString selectedFilter;
    QString fname = QFileDialog::getSaveFileName(this,
        QObject::tr("Export traces"), QString(), tr("Omicron traces (*.omtr)"),
        &selectedFilter);
    if (!fname.isEmpty())
    {
        if (!fname.mid(1).contains(QChar('.')))
            fname = fname + ".omtr";
        this->exportTraces(fname);
    }
}

void LorrisOmicron::on_actionImportTraces_triggered()
{
    QString fname = QFileDialog::getOpenFileName(this, QObject::tr("Import traces"), QString(), tr("Omicron traces (*.omtr)"));
    if (!fname.isEmpty())
        this->importTraces(fname);
}

void LorrisOmicron::importTraces(QString const & fname)
{
    QFile fin(fname);
    fin.open(QFile::ReadOnly);
    QByteArray data = fin.readAll();
    m_captured_data.assign((uint8_t const *)data.data(), (uint8_t const *)data.data() + data.size());
    this->handle_captured_data(1000000.0); // XXX
}

void LorrisOmicron::exportTraces(QString const & fname)
{
    QFile fout(fname);
    fout.open(QFile::WriteOnly | QFile::Truncate);
    fout.write((char const *)m_captured_data.data(), m_captured_data.size());
}

void LorrisOmicron::interpolationGroupTriggered(QAction * action)
{
    if (action == ui->actionInterpolationPoint)
    {
        ui->graph->setInterpolation(DigitalTraceGraph::i_point);
        sConfig.set(CFG_QUINT32_OMICRON_INTERPOLATION, 0);
    }
    else
    {
        ui->graph->setInterpolation(DigitalTraceGraph::i_linear);
        sConfig.set(CFG_QUINT32_OMICRON_INTERPOLATION, 1);
    }
}
