#ifndef TSPACKAGE_H
#define TSPACKAGE_H

#include "tstable.h"
#include "tsstream.h"

enum PACKAGE_TYPE
{
    PACKAGE_TYPE_UNKNOWN = 0,
    PACKAGE_TYPE_PSI,
    PACKAGE_TYPE_PES
};

struct TsPackage
{
    uint16_t      pid;
    uint8_t       continuity;
    PACKAGE_TYPE packageType;
    uint16_t      channel;
    bool         waitUnitStart;
    bool         hasStreamData;
    bool         streaming;
    TsStream*    pStream;
    TsTable      packageTable;

    TsPackage()
        : pid(0xffff),
        continuity(0xff),
        packageType(PACKAGE_TYPE_UNKNOWN),
        channel(0),
        waitUnitStart(true),
        hasStreamData(false),
        streaming(false),
        pStream(nullptr),
        packageTable()
    {
    }

    ~TsPackage(void)
    {
        if (pStream != nullptr) delete pStream;
    }

    void reset()
    {
        continuity = 0xff;
        waitUnitStart = true;
        packageTable.reset();
        if (pStream != nullptr)
            pStream->reset();
    }
};

#endif // TSPACKAGE_H
