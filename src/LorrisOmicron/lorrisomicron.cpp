#include "lorrisomicron.h"
#include "ui_lorrisomicron.h"
#include "../connection/omicronanalconn.h"
#include <utility>
#include <QFileDialog>
#include <QSignalMapper>

LorrisOmicron::LorrisOmicron()
    : ui(new Ui::LorrisOmicron), m_run_state(st_stopped)
{
    ui->setupUi(this);
    ui->graph->setTraceSet(&m_trace_set);

    m_connectButton = new ConnectButton(ui->connectButton);
    m_connectButton->setConnectionTypes(pct_tracelyzer);
    connect(m_connectButton, SIGNAL(connectionChosen(ConnectionPointer<Connection>)), this, SLOT(setConnection(ConnectionPointer<Connection>)));

    connect(ui->btnZoomAll, SIGNAL(clicked()), ui->graph, SLOT(zoomToAll()));
    connect(ui->btnZoomIn, SIGNAL(clicked()), ui->graph, SLOT(zoomIn()));
    connect(ui->btnClearChannels, SIGNAL(clicked()), this, SLOT(clearInputs()));

    connect(ui->btnZoomOut, SIGNAL(clicked()), ui->graph, SLOT(zoomOut()));

    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(startClicked()));
    connect(ui->btnStop, SIGNAL(clicked()), this, SLOT(stopClicked()));

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
        QMenu * menu = ui->btnAddChannel->menu();
        ui->btnAddChannel->setMenu(0);
        ui->btnAddChannel->setEnabled(false);
        delete menu;
        m_conn->disconnect(this);
    }

    m_conn.reset();
    m_run_state = st_stopped;

    if (ConnectionPointer<TracelyzerConnection> c = conn.dynamicCast<TracelyzerConnection>())
    {
        m_conn = c;
        connect(c.data(), SIGNAL(stateChanged(ConnectionState)), this, SLOT(updateUi()));
        connect(c.data(), SIGNAL(onData()), this, SLOT(onData()));

        ui->sampleFreqSpin->setMaximum(m_conn->maxFrequency());

        QMenu * inputsMenu = new QMenu(this);
        QSignalMapper * mapper = new QSignalMapper(inputsMenu);

        for (auto && kv: c->inputNames())
        {
            QAction * act = inputsMenu->addAction(QString::fromStdString(kv.second));
            mapper->setMapping(act, kv.first);
            connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
        }

        inputsMenu->addSeparator();
        QAction * act = inputsMenu->addAction(tr("Defaults"));
        connect(act, SIGNAL(triggered()), this, SLOT(addDefaultInputs()));

        connect(mapper, SIGNAL(mapped(int)), this, SLOT(addInput(int)));
        ui->btnAddChannel->setMenu(inputsMenu);
        ui->btnAddChannel->setEnabled(true);

        if (ui->graph->channelCount() == 0)
            this->addDefaultInputs();
    }

    this->updateUi();
}

void LorrisOmicron::addDefaultInputs()
{
    auto def_ids = m_conn->defaultInputs();
    for (auto && id: def_ids)
        this->addInput(id);
}

void LorrisOmicron::addInput(int id)
{
    auto const & inputNames = m_conn->inputNames();
    auto it = inputNames.find(id);
    Q_ASSERT(it != inputNames.end());
    ui->graph->addChannel(id, QString::fromUtf8(it->second.data(), it->second.size()));
}

void LorrisOmicron::clearInputs()
{
    ui->graph->clearChannels();
}

void LorrisOmicron::onData()
{
    ui->graph->update();
}

void LorrisOmicron::startClicked()
{
    Q_ASSERT(m_conn.data());
    Q_ASSERT(m_run_state == st_stopped);

    std::set<signal_trace_set::channel_id_t> channels;
    for (int ch = 0; ch < ui->graph->channelCount(); ++ch)
        channels.insert(ui->graph->input(ch).id);

    if (channels.size() > m_conn->maxChannelCount())
    {
        Utils::showErrorBox(tr("Too many inputs are selected, the device only supports %1.").arg(m_conn->maxChannelCount()));
        return;
    }

    m_conn->setChannelCount(channels.size());

    size_t muxid = 0;
    for (auto const & ch: channels)
        m_conn->setChannel(muxid++, ch);
    m_trace_set.traces.clear();
    m_conn->startTrace(m_trace_set, ui->sampleFreqSpin->value());
    this->setRunState(st_running);
}

void LorrisOmicron::stopClicked()
{
    Q_ASSERT(m_conn.data());
    Q_ASSERT(m_run_state == st_running);

    m_conn->stopTrace();
    this->setRunState(st_stopped);
}

void LorrisOmicron::setRunState(run_state_t state)
{
    m_run_state = state;
    this->updateUi();
}

void LorrisOmicron::updateUi()
{
    bool connected = m_conn && m_conn->state() == st_connected;

    if (!connected)
    {
        ui->btnStart->setEnabled(false);
        ui->btnStop->setEnabled(false);
    }
    else
    {
        switch (m_run_state)
        {
        case st_stopped:
            ui->btnStart->setEnabled(true);
            ui->btnStop->setEnabled(false);
            break;
        case st_running:
            ui->btnStart->setEnabled(false);
            ui->btnStop->setEnabled(true);
            break;
        }
    }
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
    /*QFile fin(fname);
    fin.open(QFile::ReadOnly);
    QByteArray data = fin.readAll();
    m_captured_data.assign((uint8_t const *)data.data(), (uint8_t const *)data.data() + data.size());

    size_t highestEnabled = 0;
    for (size_t i = enableBoxCount; i != 0; --i)
    {
        if (m_enableBoxes[i-1]->isChecked())
        {
            highestEnabled = i-1;
            break;
        }
    }

    size_t log_channels = 0;
    while (highestEnabled)
    {
        highestEnabled /= 2;
        ++log_channels;
    }

    this->handle_captured_data(ui->sampleFreqSpin->value(), log_channels); // XXX*/
}

void LorrisOmicron::exportTraces(QString const & fname)
{
    /*QFile fout(fname);
    fout.open(QFile::WriteOnly | QFile::Truncate);
    fout.write((char const *)m_captured_data.data(), m_captured_data.size());*/
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
