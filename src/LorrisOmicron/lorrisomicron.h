#ifndef LORRISOMICRON_H
#define LORRISOMICRON_H

#include <QWidget>
#include <QCheckBox>
#include "../WorkTab/WorkTab.h"
#include "../ui/connectbutton.h"
#include "../connection/omicronanalconn.h"
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
    void startClicked();
    void stopClicked();

    void addDefaultInputs();
    void addInput(int id);
    void clearInputs();

    void updateUi();

    void on_actionExportTraces_triggered();
    void on_actionImportTraces_triggered();

    void interpolationGroupTriggered(QAction * action);

    void onData();

private:
    Ui::LorrisOmicron *ui;

    signal_trace_set m_trace_set;

    ConnectButton * m_connectButton;
    ConnectionPointer<TracelyzerConnection> m_conn;

    enum run_state_t
    {
        st_stopped,
        st_running,
    };

    run_state_t m_run_state;
    void setRunState(run_state_t state);

    void importTraces(QString const & fname);
    void exportTraces(QString const & fname);
};

#endif // LORRISOMICRON_H
