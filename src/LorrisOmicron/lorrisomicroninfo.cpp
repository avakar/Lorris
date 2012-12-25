#include "lorrisomicroninfo.h"
#include "lorrisomicron.h"

static const LorrisOmicronInfo info;

LorrisOmicronInfo::LorrisOmicronInfo()
{
}

WorkTab *LorrisOmicronInfo::GetNewTab()
{
    return new LorrisOmicron();
}

QString LorrisOmicronInfo::GetName()
{
    return QObject::tr("Omicron Analyzer");
}

QString LorrisOmicronInfo::GetDescription()
{
    return QObject::tr("digital trace analyzer");
}

QStringList LorrisOmicronInfo::GetHandledFiles()
{
    return QStringList();
}

QString LorrisOmicronInfo::GetIdString()
{
    return "LorrisOmicron";
}
