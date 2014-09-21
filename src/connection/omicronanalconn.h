#ifndef LORRIS_CONNECTION_OMICRONANALCONN_H
#define LORRIS_CONNECTION_OMICRONANALCONN_H

#include "connection.h"
#include "../LorrisProgrammer/shupitodesc.h"
#include "../misc/threadchannel.h"
#include <libyb/usb/usb_device.hpp>
#include <libyb/usb/interface_guard.hpp>
#include <libyb/async/async_channel.hpp>
#include <libyb/async/async_runner.hpp>

class OmicronAnalyzerConnection
    : public Connection
{
    Q_OBJECT

public:
    explicit OmicronAnalyzerConnection(yb::async_runner & runner);

    QString details() const;

    void setup(yb::usb_device_interface const & intf, ShupitoDesc const & ybdesc);
    void clear();

    int vid() const;
    int pid() const;
    QString serialNumber() const;
    QString intfName() const;

    QHash<QString, QVariant> config() const;
    bool canSaveToSession() const { return true; }

protected:
    void doOpen();
    void doClose();

private:
    void closeImpl();

    yb::async_runner & m_runner;
    yb::usb_device_interface m_intf;
    yb::usb_interface_guard m_intf_guard;
    uint8_t m_out_ep;
    size_t m_out_ep_size;
    std::vector<uint8_t> m_in_eps;
    QString m_details;
    ShupitoDesc m_desc;
};

#endif // LORRIS_CONNECTION_OMICRONANALCONN_H
