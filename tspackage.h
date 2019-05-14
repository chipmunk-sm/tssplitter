#ifndef __tspackage_h__
#define __tspackage_h__

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
    quint16      pid;
    quint8       continuity;
    PACKAGE_TYPE packageType;
    quint16      channel;
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
        pStream(NULL), 
        packageTable()
    {}

    ~TsPackage(void)
    { if( pStream != NULL ) delete pStream; }

    void reset()
    {
        continuity = 0xff;
        waitUnitStart = true;
        packageTable.reset();
        if( pStream != NULL )
            pStream->reset();
    }
};

#endif // __tspackage_h__
