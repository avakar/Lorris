#ifndef LORRISOMICRONINFO_H
#define LORRISOMICRONINFO_H

#include "../WorkTab/WorkTabInfo.h"

class LorrisOmicronInfo
    : public WorkTabInfo
{
public:
    LorrisOmicronInfo();

    WorkTab *GetNewTab();
    QString GetName();
    QString GetDescription();
    QStringList GetHandledFiles();
    QString GetIdString();
};

#endif // LORRISOMICRONINFO_H
