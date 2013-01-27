#ifndef LORRISOMICRON_H
#define LORRISOMICRON_H

#include <QWidget>
#include <QCheckBox>
#include "../WorkTab/WorkTab.h"
#include "../ui/connectbutton.h"
#include <stdint.h>

namespace Ui {
class LorrisOmicron;
}

class LorrisOmicron : public WorkTab
{
    Q_OBJECT
    
public:
    LorrisOmicron();
    ~LorrisOmicron();

    QString GetIdString();

public slots:
    void setConnection(ConnectionPointer<Connection> const & conn);

private slots:
    void packetRead(ShupitoPacket const & p);
    void startStoppedClicked();

    void updateUi();

    void on_actionExportTraces_triggered();
    void on_actionImportTraces_triggered();

    void interpolationGroupTriggered(QAction * action);

private:
    Ui::LorrisOmicron *ui;

    static size_t const enableBoxCount = 16;
    QCheckBox * m_enableBoxes[enableBoxCount];

    ConnectButton * m_connectButton;
    ConnectionPointer<ShupitoConnection> m_conn;

    enum run_state_t
    {
        st_stopped,
        st_start_requested,
        st_running,
        st_stop_requested,
        st_reading_samples
    };

    run_state_t m_run_state;
    void setRunState(run_state_t state);

    uint32_t m_period;
    uint8_t m_log_channels;

    uint32_t m_start_index;
    uint32_t m_start_addr;
    uint32_t m_stop_index;
    uint32_t m_stop_addr;
    uint16_t m_requested_length;
    bool m_readCanceled;
    void readNextChunk();

    void readMem(uint32_t addr, uint16_t len);

    std::vector<uint8_t> m_captured_data;
    void handle_captured_data(double samples_per_second, int log_channels);

    void importTraces(QString const & fname);
    void exportTraces(QString const & fname);
};

#endif // LORRISOMICRON_H
