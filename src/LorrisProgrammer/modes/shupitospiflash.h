/**********************************************
**    This file is part of Lorris
**    http://tasssadar.github.com/Lorris/
**
**    See README and COPYING
***********************************************/

#ifndef SHUPITOSPIFLASH_H
#define SHUPITOSPIFLASH_H

#include "shupitomode.h"
#include <stdint.h>

class ShupitoSpiFlashBase : public ShupitoMode
{
    Q_OBJECT
public:
    ShupitoSpiFlashBase(Shupito *shupito);

    virtual chip_definition readDeviceId() override;
    virtual void erase_device(chip_definition& chip) override;

    ProgrammerCapabilities capabilities() const override;

protected:
    virtual void readMemRange(quint8 memid, QByteArray& memory, quint32 address, quint32 size) override;
    virtual void flashPage(chip_definition::memorydef *memdef, std::vector<quint8>& memory, quint32 address) override;

    virtual void transfer(uint8_t * data, size_t size, bool first, bool last) = 0;
    virtual size_t maxTransferSize() const = 0;

private:
    void writeEnable();
    uint8_t readStatus();

    void readSfdp(uint32_t addr, uint8_t * data, size_t size);
};

class ShupitoSpiFlash : public ShupitoSpiFlashBase
{
    Q_OBJECT
public:
    ShupitoSpiFlash(Shupito *shupito);

protected:
    ShupitoDesc::config const *getModeCfg() override;
    void transfer(uint8_t * data, size_t size, bool first, bool last) override;
    size_t maxTransferSize() const override;
};

class ShupitoJtagSpiFlash : public ShupitoSpiFlashBase
{
    Q_OBJECT
public:
    ShupitoJtagSpiFlash(Shupito *shupito);

    void switchToFlashMode(quint32 speed_hz) override;
    void switchToRunMode() override;

protected:
    ShupitoDesc::config const *getModeCfg() override;
    void transfer(uint8_t * data, size_t size, bool first, bool last) override;
    size_t maxTransferSize() const override;
};

#endif // SHUPITOSPIFLASH_H
