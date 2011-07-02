#ifndef WORKTABINFO_H
#define WORKTABINFO_H

#include <QString>
#include "connection/connectionmgr.h"

class WorkTab;

class WorkTabInfo
{
    public:
        virtual ~WorkTabInfo();

        virtual WorkTab *GetNewTab();
        virtual QString GetName();
        virtual uint8_t GetConType();

    protected:
        explicit WorkTabInfo();

};

#endif // WORKTABINFO_H
