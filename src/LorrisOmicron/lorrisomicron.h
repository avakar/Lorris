#ifndef LORRISOMICRON_H
#define LORRISOMICRON_H

#include <QWidget>
#include "../WorkTab/WorkTab.h"
#include "../ui/connectbutton.h"

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

private:
    Ui::LorrisOmicron *ui;

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

    uint32_t m_start_index;
    uint32_t m_start_addr;
    uint32_t m_stop_index;
    uint32_t m_stop_addr;
    uint16_t m_requested_length;
    void readNextChunk();

    void readMem(uint32_t addr, uint16_t len);

    std::vector<uint8_t> m_captured_data;
    void handle_captured_data();
};

#endif // LORRISOMICRON_H
