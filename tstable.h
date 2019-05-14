#ifndef __tstable_h__
#define __tstable_h__

#include <QString>

// PSI section size (EN 300 468)
#define TABLE_BUFFER_SIZE       4096

struct TsTable
{
    quint8  tableId;
    quint8  version;
    quint16 id;
    quint16 len;
    quint16 offset;
    quint8  buf[TABLE_BUFFER_SIZE];

    TsTable() 
        : tableId(0xff), 
        version(0xff), 
        id(0xffff), 
        len(0), 
        offset(0)
    { memset(buf, 0, TABLE_BUFFER_SIZE); }

    void reset(void) {
        len = 0;
        offset = 0;
    }
};

#endif // __tstable_h__

