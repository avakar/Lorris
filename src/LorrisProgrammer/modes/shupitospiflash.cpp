/**********************************************
**    This file is part of Lorris
**    http://tasssadar.github.com/Lorris/
**
**    See README and COPYING
***********************************************/

#include "../shupito.h"
#include "shupitospiflash.h"
#include "../../shared/defmgr.h"

//---------------------------------------------------------------------
ShupitoSpiFlashBase::ShupitoSpiFlashBase(Shupito *shupito)
    : ShupitoMode(shupito)
{
}

void ShupitoSpiFlashBase::readSfdp(uint32_t addr, uint8_t * data, size_t size)
{
    std::vector<uint8_t> out(size + 5, 0);
    out[0] = 0x5a;
    out[1] = addr >> 16;
    out[2] = addr >> 8;
    out[3] = addr;

    this->transfer(out.data(), size + 5, true, true);
    std::copy(out.begin() + 5, out.end(), data);
}

chip_definition ShupitoSpiFlashBase::readDeviceId()
{
    chip_definition cd;

    uint32_t flash_size_bytes = 0;
    {
        uint8_t req[] = { 0x9f, 0, 0, 0 };

        this->transfer(req, sizeof req, true, true);
        if (req[1] == 0 && req[2] == 0 && req[3] == 0)
            throw QString(tr("No flash device detected"));

        char sign[] = "spiflash:000000";
        Utils::toBase16(sign + 9, req[1]);
        Utils::toBase16(sign + 11, req[2]);
        Utils::toBase16(sign + 13, req[3]);
        cd.setSign(sign);
        cd.setName(sign);

        flash_size_bytes = (1<<(req[3] % 32));
    }

    uint8_t header[16];
    this->readSfdp(0, header, sizeof header);

    if (header[0] == 'S' && header[1] == 'F' && header[2] == 'D' && header[3] == 'P')
    {
        uint32_t table_address = deserialize_le<uint32_t>(header + 0xc, 3);

        uint8_t flash_params[16];
        this->readSfdp(table_address, flash_params, sizeof flash_params);

        uint32_t flash_size_bits = deserialize_le<uint32_t>(flash_params + 4);
        flash_size_bytes = (flash_size_bits + 1) / 8;
    }

    QHash<QString, chip_definition::memorydef> & mds = cd.getMems();
    chip_definition::memorydef & md = mds["flash"];
    md.memid = 1;
    md.size = flash_size_bytes;
    md.pagesize = 256;

    sDefMgr.update(cd);
    return cd;
}

void ShupitoSpiFlashBase::readMemRange(quint8 memid, QByteArray& memory, quint32 address, quint32 size)
{
    std::vector<uint8_t> out;
    out.push_back(3);
    out.push_back(address >> 16);
    out.push_back(address >> 8);
    out.push_back(address);

    bool first = true;
    while (size && !m_cancel_requested)
    {
        size_t offset = out.size();
        size_t chunk = (std::min)((size_t)size, this->maxTransferSize() - (out.size() + 1));
        size -= chunk;

        out.resize(chunk + offset, 0);

        this->transfer(out.data(), out.size(), first, size == 0);
        memory.append((char *)out.data() + offset, out.size() - offset);

        out.clear();

        first = false;
    }
}

void ShupitoSpiFlashBase::flashPage(chip_definition::memorydef *memdef, std::vector<quint8>& memory, quint32 address)
{
    this->writeEnable();
    if ((this->readStatus() & (1<<1)) == 0)
        throw tr("Failed to enable write");

    std::vector<uint8_t> out;
    out.push_back(2);
    out.push_back(address >> 16);
    out.push_back(address >> 8);
    out.push_back(address);
    out.insert(out.end(), memory.begin(), memory.end());
    this->transfer(out.data(), out.size(), true, true);

    while (this->readStatus() & (1<<0))
    {
    }

    if ((this->readStatus() & (1<<1)) != 0)
        throw tr("The device didn't reset the write enable latch");
}

void ShupitoSpiFlashBase::erase_device(chip_definition& chip)
{
    this->writeEnable();
    if ((this->readStatus() & (1<<1)) == 0)
        throw tr("Failed to enable write");

    uint8_t req[] = { 0xC7 };
    this->transfer(req, sizeof req, true, true);

    while (this->readStatus() & (1<<0))
    {
    }

    if ((this->readStatus() & (1<<1)) != 0)
        throw tr("The device didn't reset the write enable latch");
}

void ShupitoSpiFlashBase::writeEnable()
{
    uint8_t req[] = { 6 };
    this->transfer(req, sizeof req, true, true);
}

uint8_t ShupitoSpiFlashBase::readStatus()
{
    uint8_t req[] = { 5, 0 };
    this->transfer(req, sizeof req, true, true);
    return req[1];
}

ProgrammerCapabilities ShupitoSpiFlashBase::capabilities() const
{
    ProgrammerCapabilities ps;
    ps.flash = true;
    return ps;
}

//---------------------------------------------------------------------
ShupitoSpiFlash::ShupitoSpiFlash(Shupito *shupito)
    : ShupitoSpiFlashBase(shupito)
{
}

ShupitoDesc::config const * ShupitoSpiFlash::getModeCfg()
{
    return m_shupito->getDesc()->getConfig("633125ab-32e0-49ec-b240-7d845bb70b2d");
}

void ShupitoSpiFlash::transfer(uint8_t * data, size_t size, bool /*first*/, bool last)
{
    size_t ms = m_shupito->maxPacketSize() - 1;

    while (size)
    {
        size_t chunk = (std::min)(ms, size);

        uint8_t flags = 0;
        if (chunk == size && last)
            flags |= 0x02;

        ShupitoPacket pkt;
        pkt.push_back(m_prog_cmd_base + 2);
        pkt.push_back(flags);
        pkt.insert(pkt.end(), data, data + chunk);
        pkt = m_shupito->waitForPacket(pkt, m_prog_cmd_base + 2);

        if (pkt.size() != chunk + 2)
            throw QString(tr("Invalid response."));

        std::copy(pkt.begin() + 2, pkt.end(), data);

        data += chunk;
        size -= chunk;
    }
}

size_t ShupitoSpiFlash::maxTransferSize() const
{
    return m_shupito->maxPacketSize() - 2;
}

//---------------------------------------------------------------------
ShupitoJtagSpiFlash::ShupitoJtagSpiFlash(Shupito *shupito)
    : ShupitoSpiFlashBase(shupito)
{
}

void ShupitoJtagSpiFlash::switchToFlashMode(quint32 speed_hz)
{
    m_flash_mode = false;

    ShupitoDesc::config const *prog_cfg = getModeCfg();
    if (!prog_cfg)
        throw QString(QObject::tr("The device can't program these types of chips."));

    m_shupito->sendPacket(prog_cfg->getStateChangeCmd(true));

    uint32_t freq_base = 0;
    if (prog_cfg->data.size() >= 5 && prog_cfg->data[0] == 1)
        freq_base = deserialize_le<uint32_t>(prog_cfg->data.data() + 1);

    m_prog_cmd_base = prog_cfg->cmd;

    if (freq_base != 0)
    {
        uint32_t max_freq = speed_hz;
        if (prog_cfg->data.size() >= 9 && prog_cfg->data[0] == 1)
            max_freq = deserialize_le<uint32_t>(prog_cfg->data.data() + 5);

        uint32_t max_freq_hz = (std::min)(speed_hz, max_freq);
        uint32_t period = (freq_base + max_freq_hz - 1) / max_freq_hz;

        uint8_t packet[5] = { m_prog_cmd_base + 2 };
        serialize_le(packet + 1, period);
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }

    // go to pause-ir
    {
        // 11111111 to go reset, 011010 to pause-ir
        uint8_t packet[] = { m_prog_cmd_base + 0, 8+6, 0xff, 0x16 };
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }


    // shift 6'd2 to IR
    {
        uint8_t packet[] = { m_prog_cmd_base + 1, 0x10/*no_verify*/ | 6, 2 };
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }

    // go to idle
    {
        // pause-ir -> idle: 110
        uint8_t packet[] = { m_prog_cmd_base + 0, 3, 3 };
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }

    m_flash_mode = true;
}

void ShupitoJtagSpiFlash::switchToRunMode()
{
    if (!m_flash_mode)
        return;

    {
        // 111111 to go reset
        uint8_t packet[] = { m_prog_cmd_base + 0, 6, 0x3f };
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }

    ShupitoDesc::config const *prog_cfg = getModeCfg();
    Q_ASSERT(prog_cfg != 0);
    m_shupito->sendPacket(prog_cfg->getStateChangeCmd(false));
    m_flash_mode = false;
}

ShupitoDesc::config const * ShupitoJtagSpiFlash::getModeCfg()
{
    return m_shupito->getDesc()->getConfig("fe047e35-dec8-48ab-b194-e3762c8f6b66");
}

void ShupitoJtagSpiFlash::transfer(uint8_t * data, size_t size, bool first, bool last)
{
    static uint16_t const conv[] = {
        0x0000, 0x4000, 0x1000, 0x5000, 0x0400, 0x4400, 0x1400, 0x5400,
        0x0100, 0x4100, 0x1100, 0x5100, 0x0500, 0x4500, 0x1500, 0x5500,
        0x0040, 0x4040, 0x1040, 0x5040, 0x0440, 0x4440, 0x1440, 0x5440,
        0x0140, 0x4140, 0x1140, 0x5140, 0x0540, 0x4540, 0x1540, 0x5540,
        0x0010, 0x4010, 0x1010, 0x5010, 0x0410, 0x4410, 0x1410, 0x5410,
        0x0110, 0x4110, 0x1110, 0x5110, 0x0510, 0x4510, 0x1510, 0x5510,
        0x0050, 0x4050, 0x1050, 0x5050, 0x0450, 0x4450, 0x1450, 0x5450,
        0x0150, 0x4150, 0x1150, 0x5150, 0x0550, 0x4550, 0x1550, 0x5550,
        0x0004, 0x4004, 0x1004, 0x5004, 0x0404, 0x4404, 0x1404, 0x5404,
        0x0104, 0x4104, 0x1104, 0x5104, 0x0504, 0x4504, 0x1504, 0x5504,
        0x0044, 0x4044, 0x1044, 0x5044, 0x0444, 0x4444, 0x1444, 0x5444,
        0x0144, 0x4144, 0x1144, 0x5144, 0x0544, 0x4544, 0x1544, 0x5544,
        0x0014, 0x4014, 0x1014, 0x5014, 0x0414, 0x4414, 0x1414, 0x5414,
        0x0114, 0x4114, 0x1114, 0x5114, 0x0514, 0x4514, 0x1514, 0x5514,
        0x0054, 0x4054, 0x1054, 0x5054, 0x0454, 0x4454, 0x1454, 0x5454,
        0x0154, 0x4154, 0x1154, 0x5154, 0x0554, 0x4554, 0x1554, 0x5554,
        0x0001, 0x4001, 0x1001, 0x5001, 0x0401, 0x4401, 0x1401, 0x5401,
        0x0101, 0x4101, 0x1101, 0x5101, 0x0501, 0x4501, 0x1501, 0x5501,
        0x0041, 0x4041, 0x1041, 0x5041, 0x0441, 0x4441, 0x1441, 0x5441,
        0x0141, 0x4141, 0x1141, 0x5141, 0x0541, 0x4541, 0x1541, 0x5541,
        0x0011, 0x4011, 0x1011, 0x5011, 0x0411, 0x4411, 0x1411, 0x5411,
        0x0111, 0x4111, 0x1111, 0x5111, 0x0511, 0x4511, 0x1511, 0x5511,
        0x0051, 0x4051, 0x1051, 0x5051, 0x0451, 0x4451, 0x1451, 0x5451,
        0x0151, 0x4151, 0x1151, 0x5151, 0x0551, 0x4551, 0x1551, 0x5551,
        0x0005, 0x4005, 0x1005, 0x5005, 0x0405, 0x4405, 0x1405, 0x5405,
        0x0105, 0x4105, 0x1105, 0x5105, 0x0505, 0x4505, 0x1505, 0x5505,
        0x0045, 0x4045, 0x1045, 0x5045, 0x0445, 0x4445, 0x1445, 0x5445,
        0x0145, 0x4145, 0x1145, 0x5145, 0x0545, 0x4545, 0x1545, 0x5545,
        0x0015, 0x4015, 0x1015, 0x5015, 0x0415, 0x4415, 0x1415, 0x5415,
        0x0115, 0x4115, 0x1115, 0x5115, 0x0515, 0x4515, 0x1515, 0x5515,
        0x0055, 0x4055, 0x1055, 0x5055, 0x0455, 0x4455, 0x1455, 0x5455,
        0x0155, 0x4155, 0x1155, 0x5155, 0x0555, 0x4555, 0x1555, 0x5555,
    };

    static uint8_t const inv_conv[] = {
        0x0, 0x0, 0x8, 0x8, 0x0, 0x0, 0x8, 0x8, 0x4, 0x4, 0xc, 0xc, 0x4, 0x4, 0xc, 0xc,
        0x0, 0x0, 0x8, 0x8, 0x0, 0x0, 0x8, 0x8, 0x4, 0x4, 0xc, 0xc, 0x4, 0x4, 0xc, 0xc,
        0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0xa, 0xa, 0x6, 0x6, 0xe, 0xe, 0x6, 0x6, 0xe, 0xe,
        0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0xa, 0xa, 0x6, 0x6, 0xe, 0xe, 0x6, 0x6, 0xe, 0xe,
        0x0, 0x0, 0x8, 0x8, 0x0, 0x0, 0x8, 0x8, 0x4, 0x4, 0xc, 0xc, 0x4, 0x4, 0xc, 0xc,
        0x0, 0x0, 0x8, 0x8, 0x0, 0x0, 0x8, 0x8, 0x4, 0x4, 0xc, 0xc, 0x4, 0x4, 0xc, 0xc,
        0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0xa, 0xa, 0x6, 0x6, 0xe, 0xe, 0x6, 0x6, 0xe, 0xe,
        0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0xa, 0xa, 0x6, 0x6, 0xe, 0xe, 0x6, 0x6, 0xe, 0xe,
        0x1, 0x1, 0x9, 0x9, 0x1, 0x1, 0x9, 0x9, 0x5, 0x5, 0xd, 0xd, 0x5, 0x5, 0xd, 0xd,
        0x1, 0x1, 0x9, 0x9, 0x1, 0x1, 0x9, 0x9, 0x5, 0x5, 0xd, 0xd, 0x5, 0x5, 0xd, 0xd,
        0x3, 0x3, 0xb, 0xb, 0x3, 0x3, 0xb, 0xb, 0x7, 0x7, 0xf, 0xf, 0x7, 0x7, 0xf, 0xf,
        0x3, 0x3, 0xb, 0xb, 0x3, 0x3, 0xb, 0xb, 0x7, 0x7, 0xf, 0xf, 0x7, 0x7, 0xf, 0xf,
        0x1, 0x1, 0x9, 0x9, 0x1, 0x1, 0x9, 0x9, 0x5, 0x5, 0xd, 0xd, 0x5, 0x5, 0xd, 0xd,
        0x1, 0x1, 0x9, 0x9, 0x1, 0x1, 0x9, 0x9, 0x5, 0x5, 0xd, 0xd, 0x5, 0x5, 0xd, 0xd,
        0x3, 0x3, 0xb, 0xb, 0x3, 0x3, 0xb, 0xb, 0x7, 0x7, 0xf, 0xf, 0x7, 0x7, 0xf, 0xf,
        0x3, 0x3, 0xb, 0xb, 0x3, 0x3, 0xb, 0xb, 0x7, 0x7, 0xf, 0xf, 0x7, 0x7, 0xf, 0xf,
    };

    if (first)
    {
        // idle -> pause-dr: 1010
        uint8_t packet[] = { m_prog_cmd_base + 0, 4, 5 };
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }

    std::vector<uint8_t> req;
    req.push_back(m_prog_cmd_base + 1);
    req.push_back(0);
    while (size)
    {
        size_t chunk = (std::min)(size, (m_shupito->maxPacketSize() - 2) / 2);

        req.resize(2*chunk + 2);
        for (size_t i = 0; i < chunk; ++i)
        {
            req[2*i+2] = conv[data[i]];
            req[2*i+3] = conv[data[i]] >> 8;
        }

        ShupitoPacket res = m_shupito->waitForPacket(req, m_prog_cmd_base + 1);
        if (res.size() != req.size())
            throw QString(tr("Invalid response."));

        for (size_t i = 2; i < res.size(); i += 2)
            *data++ = (inv_conv[res[i]] << 4) | inv_conv[res[i+1]];

        size -= chunk;
    }

    if (last)
    {
        // pause-dr -> idle: 110
        uint8_t packet[] = { m_prog_cmd_base + 0, 3, 3 };
        m_shupito->waitForPacket(ShupitoPacket(packet, packet + sizeof packet), packet[0]);
    }
}

size_t ShupitoJtagSpiFlash::maxTransferSize() const
{
    return (m_shupito->maxPacketSize() - 2) / 2;
}
