#include "omicronanalconn.h"
#include "../misc/utils.h"
#include <stdexcept>
#include <QEvent>
#include <QCoreApplication>

OmicronAnalyzerConnection::OmicronAnalyzerConnection(yb::async_runner & runner)
    : Connection(CONNECTION_OMICRON_ANALYZER), m_runner(runner)
{
    this->markMissing();
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

void OmicronAnalyzerConnection::setup(yb::usb_device_interface const & intf, ShupitoDesc const & ybdesc)
{
    assert(!intf.empty());
    yb::usb_interface const & idesc = intf.descriptor();
    assert(idesc.altsettings.size() == 1);

    yb::usb_interface_descriptor const & desc = idesc.altsettings[0];

    m_intf = intf;
    m_out_ep = 0;
    m_in_eps.clear();

    for (size_t i = 0; i < desc.endpoints.size(); ++i)
    {
        if (desc.endpoints[i].is_output())
        {
            m_out_ep = desc.endpoints[i].bEndpointAddress;
            m_out_ep_size = desc.endpoints[i].wMaxPacketSize;
        }
        else
            m_in_eps.push_back(desc.endpoints[i].bEndpointAddress);
    }

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
    if (this->state() == st_disconnecting)
    {
        this->closeImpl();
        this->SetState(st_disconnected);
    }
    else
    {
        emit disconnecting();
        this->SetState(st_disconnecting);
    }
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
