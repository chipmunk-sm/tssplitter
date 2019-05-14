#ifndef TSTABLE_H
#define TSTABLE_H

#include <QString>

// PSI section size (EN 300 468)
#define TABLE_BUFFER_SIZE       4096

struct TsTable
{
    uint8_t  tableId;
    uint8_t  version;
    uint16_t id;
    uint16_t len;
    uint16_t offset;
    uint8_t  buf[TABLE_BUFFER_SIZE];

    TsTable()
        : tableId(0xff),
        version(0xff),
        id(0xffff),
        len(0),
        offset(0)
    {
        memset(buf, 0, TABLE_BUFFER_SIZE);
    }

    void reset(void)
    {
        len = 0;
        offset = 0;
    }
};

#endif // TSTABLE_H

