#include "tscontext.h"
#include "ts_mpegvideo.h"
#include "ts_mpegaudio.h"
#include "ts_aac.h"
#include "ts_ac3.h"
#include "ts_h264.h"
#include "ts_subtitle.h"
#include "ts_teletext.h"

#define MAX_RESYNC_SIZE 65536

////////////////////////////////////////////////////////////////////////////////
AVContext::AVContext(TsParser& parser, const qint64& pos, quint16 channel)
    : parser_(parser),
    avPos_(pos), 
    avDataLen_(FLUTS_NORMAL_TS_PACKAGESIZE), 
    avPkgSize_(0), 
    isConfigured_(false), 
    channel_(channel), 
    pid_(0xffff), 
    transportError_(false), 
    hasPayload_(false), 
    payloadUnitStart_(false), 
    discontinuity_(false), 
    payload_(NULL), 
    payloadLen_(0), 
    package_(NULL),
    csMutex_(new QMutex())
{
    memset(avBuf_, 0, sizeof(avBuf_));
}

AVContext::~AVContext()
{
    reset();
    delete csMutex_;
}

void AVContext::reset()
{
    QMutexLocker lock(csMutex_);

    pid_ = 0xffff;
    transportError_ = false;
    hasPayload_ = false;
    payloadUnitStart_ = false;
    discontinuity_ = false;
    payload_ = NULL;
    payloadLen_ = 0;
    package_ = NULL;
}

QVector<TsStream*> AVContext::getStreams() const
{
    QMutexLocker lock(csMutex_);

    QVector<TsStream*> v;
    QMap<quint16,TsPackage>::const_iterator It = packages_.begin();
    for(; It != packages_.end(); ++It)
        if( It->packageType == PACKAGE_TYPE_PES && It->pStream != NULL )
            v.push_back(It->pStream);
    return v;
}

void AVContext::startStreaming(quint16 pid)
{
    QMutexLocker lock(csMutex_);
    QMap<quint16,TsPackage>::iterator It = packages_.find(pid);
    if( It != packages_.end() )
        It->streaming = true;
}

void AVContext::stopStreaming(quint16 pid)
{
    QMutexLocker lock(csMutex_);
    QMap<quint16,TsPackage>::iterator It = packages_.find(pid);
    if( It != packages_.end() )
        It->streaming = false;
}

////////////////////////////////////////////////////////////////////////////////
//  MPEG-TS parser for the context
STREAM_TYPE AVContext::getStreamType(quint8 pesType)
{
    switch( pesType )
    {
    case 0x01:
        return STREAM_TYPE_VIDEO_MPEG1;
    case 0x02:
        return STREAM_TYPE_VIDEO_MPEG2;
    case 0x03:
        return STREAM_TYPE_AUDIO_MPEG1;
    case 0x04:
        return STREAM_TYPE_AUDIO_MPEG2;
    case 0x06:
        return STREAM_TYPE_PRIVATE_DATA;
    case 0x0f:
    case 0x11:
        return STREAM_TYPE_AUDIO_AAC;
    case 0x10:
        return STREAM_TYPE_VIDEO_MPEG4;
    case 0x1b:
        return STREAM_TYPE_VIDEO_H264;
    case 0xea:
        return STREAM_TYPE_VIDEO_VC1;
    case 0x80:
        return STREAM_TYPE_AUDIO_LPCM;
    case 0x81:
    case 0x83:
    case 0x84:
    case 0x87:
        return STREAM_TYPE_AUDIO_AC3;
    case 0x82:
    case 0x85:
    case 0x8a:
        return STREAM_TYPE_AUDIO_DTS;
    }
    return STREAM_TYPE_UNKNOWN;
}

qint32 AVContext::configureTs()
{
    const quint8* data;
    qint32 dataSize = AV_CONTEXT_PACKAGESIZE;
    qint64 pos = avPos_;
    qint32 fluts[][2] = {
        {FLUTS_NORMAL_TS_PACKAGESIZE, 0},
        {FLUTS_M2TS_TS_PACKAGESIZE, 0},
        {FLUTS_DVB_ASI_TS_PACKAGESIZE, 0},
        {FLUTS_ATSC_TS_PACKAGESIZE, 0}
    };
    qint32 nb = sizeof(fluts) / (2*sizeof(qint32));
    qint32 score = TS_CHECK_MIN_SCORE;

    for(qint32 i = 0; i < MAX_RESYNC_SIZE; i++)
    {
        if( NULL == (data = parser_.read(pos, dataSize)) )
            return AVCONTEXT_IO_ERROR;

        if( data[0] == 0x47 )
        {
            qint32 count, found;
            for (qint32 t = 0; t < nb; t++) // for all fluts
            {
                const quint8* ndata;
                qint64 npos = pos;
                qint32 do_retry = score; // reach for score
                do {
                    --do_retry;
                    npos += fluts[t][0];
                    if( NULL == (ndata = parser_.read(npos, dataSize)) )
                        return AVCONTEXT_IO_ERROR;
                }
                while( ndata[0] == 0x47 && (++fluts[t][1]) && do_retry );
            }

            // Is score reached ?
            count = found = 0;
            for (qint32 t = 0; t < nb; t++)
            {
                if( fluts[t][1] == score ) {
                    found = t;
                    ++count;
                }
                // Reset score for next retry
                fluts[t][1] = 0;
            }

            // One and only one is eligible
            if( count == 1 )
            {
                avPkgSize_ = fluts[found][0];
                avPos_ = pos;
                return AVCONTEXT_CONTINUE;
            }
            // More one: Retry for highest score
            else if (count > 1 && ++score > TS_CHECK_MAX_SCORE)
                // Package size remains undetermined
                break;
            else // None: Bad sync. Shift and retry
                pos++;
        }
        else
        pos++;
    }
    // stream is invalid
    return AVCONTEXT_TS_NOSYNC;
}

qint32 AVContext::TSResync()
{
    const quint8* data;
    if( !isConfigured_ )
    {
        qint32 ret = configureTs();
        if( ret != AVCONTEXT_CONTINUE )
            return ret;
        isConfigured_ = true;
    }

    for(qint32 i = 0; i < MAX_RESYNC_SIZE; i++)
    {
        if( NULL == (data = parser_.read(avPos_, avPkgSize_)) )
            return AVCONTEXT_IO_ERROR;
        if( data[0] == 0x47 ) {
            memcpy(avBuf_, data, avPkgSize_);
            reset();
            return AVCONTEXT_CONTINUE;
        }
        avPos_++;
    }
    return AVCONTEXT_TS_NOSYNC;
}

//////////////////////////////////////////////////////////////////////////
// Process TS package
// returns:
// AVCONTEXT_CONTINUE
// Parse completed. If has payload, process it else Continue to next package.
// AVCONTEXT_STREAM_PID_DATA
// Parse completed. A new PES unit starts and data of elementary stream for
// the PID must be picked before processing this payload.
// AVCONTEXT_DISCONTINUITY
// Discontinuity. PID will wait until next unit start. So continue to next package.
// AVCONTEXT_TS_NOSYNC
// Bad sync byte. Should run TSResync().
// AVCONTEXT_TS_ERROR
// Parsing error
qint32 AVContext::processTSPackage()
{
    QMutexLocker lock(csMutex_);

    qint32 ret = AVCONTEXT_CONTINUE;
    QMap<quint16,TsPackage>::iterator It;

    if( avRb8(avBuf_) != 0x47) // ts sync byte
        return AVCONTEXT_TS_NOSYNC;

    quint16 header = avRb16(avBuf_+1);
    pid_ = header & 0x1fff;
    transportError_ = (header & 0x8000) != 0;
    payloadUnitStart_ = (header & 0x4000) != 0;
    // Cleaning context
    discontinuity_ = false;
    hasPayload_ = false;
    payload_ = NULL;
    payloadLen_ = 0;

    if( transportError_ )
        return AVCONTEXT_CONTINUE;
    // Null package
    if( pid_ == 0x1fff )
        return AVCONTEXT_CONTINUE;

    quint8 flags = avRb8(avBuf_+3);
    bool hasPayload = (flags & 0x10) != 0;
    bool isDiscontinuity = false;
    quint8 continuityCounter = flags & 0x0f;
    bool hasAdaptation = (flags & 0x20) != 0;
    qint32 n = 0;
    if( hasAdaptation )
    {
        qint32 len = (qint32)avRb8(avBuf_+ 4);
        if( len > avDataLen_-5)
            return AVCONTEXT_TS_ERROR;
        n = len + 1;
        if( len > 0 )
            isDiscontinuity = (avRb8(avBuf_+5) & 0x80) != 0;
    }

    if( hasPayload )
    {
        // Payload start after adaptation fields
        payload_ = avBuf_ + n + 4;
        payloadLen_ = avDataLen_ - n - 4;
    }

    It = packages_.find(pid_);
    if( It == packages_.end() )
    {
        // Not registred PID
        // We are waiting for unit start of PID 0 else next package is required
        if( pid_ == 0 && payloadUnitStart_ )
        {
            // Registering PID 0
            TsPackage pid0;
            pid0.pid = pid_;
            pid0.packageType = PACKAGE_TYPE_PSI;
            pid0.continuity = continuityCounter;
            It = packages_.insert(It, pid_, pid0);
        }
        else
            return AVCONTEXT_CONTINUE;
    }
    else
    {
        // PID is registred
        // Checking unit start is required
        if( It->waitUnitStart && !payloadUnitStart_)
        {
            // Not unit start. Save package flow continuity...
            It->continuity = continuityCounter;
            discontinuity_ = true;
            return AVCONTEXT_DISCONTINUITY;
        }

        // Checking continuity where possible
        if( It->continuity != 0xff )
        {
            quint8 expected_cc = hasPayload ? (It->continuity + 1) & 0x0f : It->continuity;
            if( !isDiscontinuity && expected_cc != continuityCounter )
            {
                discontinuity_ = true;
                // If unit is not start then reset PID and wait the next unit start
                if( !payloadUnitStart_ ) {
                    It->reset();
                    return AVCONTEXT_DISCONTINUITY;
                }
            }
        }
        It->continuity = continuityCounter;
    }

    discontinuity_ |= isDiscontinuity;
    hasPayload_ = hasPayload;
    package_ = &(It.value());

    // It is time to stream data for PES
    if( payloadUnitStart_ &&
        package_->streaming &&
        package_->packageType == PACKAGE_TYPE_PES &&
        !package_->waitUnitStart )
    {
        package_->hasStreamData = true;
        ret = AVCONTEXT_STREAM_PID_DATA;
    }
    return ret;
}

// Process payload of package depending of its type
// PACKAGE_TYPE_PSI -> parseTsPsi()
// PACKAGE_TYPE_PES -> parseTsPes()
qint32 AVContext::processTSPayload()
{
    QMutexLocker lock(csMutex_);

    if( !package_ )
        return AVCONTEXT_CONTINUE;

    qint32 ret = 0;
    switch( package_->packageType ) {
    case PACKAGE_TYPE_PSI: 
        ret = parseTsPsi(); 
        break;
    case PACKAGE_TYPE_PES: 
        ret = parseTsPes(); 
        break;
    case PACKAGE_TYPE_UNKNOWN: 
        break;
    }
    return ret;
}

void AVContext::clearPmt()
{
    QVector<quint16> pidList;
    QMap<quint16, TsPackage>::iterator It = packages_.begin();
    for(; It != packages_.end(); ++It)
        if( It->packageType == PACKAGE_TYPE_PSI && 
            It->packageTable.tableId == 0x02 )
        {
            pidList.push_back(It.key());
            clearPes(It->channel);
        }

    for( QVector<quint16>::iterator vIt = pidList.begin(); vIt != pidList.end(); ++vIt )
        if( packages_.end() != (It = packages_.find(*vIt)) )
            packages_.erase(It);
}

void AVContext::clearPes(quint16 channel)
{
    QVector<quint16> pidList;
    QMap<quint16, TsPackage>::iterator It = packages_.begin();
    for(; It != packages_.end(); ++It)
        if( It->packageType == PACKAGE_TYPE_PES && 
            It->channel == channel )
        {
            pidList.push_back(It.key());
        }

    for( QVector<quint16>::iterator vIt = pidList.begin(); vIt != pidList.end(); ++vIt )
        if( packages_.end() != (It = packages_.find(*vIt)) )
            packages_.erase(It);
}

// Parse PSI payload
// returns:
// AVCONTEXT_CONTINUE
// Parse completed. Continue to next package
// AVCONTEXT_PROGRAM_CHANGE
// Parse completed. The program has changed. All streams are resetted and
// streaming flag is set to false. Client must inspect streams MAP and enable
// streaming for those recognized.
// AVCONTEXT_TS_ERROR
// Parsing error
qint32 AVContext::parseTsPsi()
{
    qint32 len;
    if( !hasPayload_ || payload_ == NULL || payloadLen_ == 0 || package_ == NULL )
        return AVCONTEXT_CONTINUE;

    if( payloadUnitStart_ )
    {
        // reset wait for unit start
        package_->waitUnitStart = false;
        // pointer field present
        len = (qint32)avRb8(payload_);
        if( len > payloadLen_ )
            return AVCONTEXT_TS_ERROR;
        // table ID
        quint8 tableId = avRb8(payload_+1);
        // table length
        len = (qint32)avRb16(payload_+2);
        if( (len & 0x3000) != 0x3000 )
            return AVCONTEXT_TS_ERROR;

        len &= 0x0fff;
        package_->packageTable.reset();

        qint32 n = payloadLen_ - 4;
        memcpy(package_->packageTable.buf, payload_+4, n);
        package_->packageTable.tableId = tableId;
        package_->packageTable.offset = n;
        package_->packageTable.len = len;
        // check for incomplete section
        if( package_->packageTable.offset < package_->packageTable.len)
            return AVCONTEXT_CONTINUE;
    }
    else
    {
        // next part of PSI
        if( package_->packageTable.offset == 0 )
            return AVCONTEXT_TS_ERROR;

        if( (payloadLen_ + package_->packageTable.offset) > TABLE_BUFFER_SIZE )
            return AVCONTEXT_TS_ERROR;

        memcpy(package_->packageTable.buf + package_->packageTable.offset, payload_, payloadLen_);
        package_->packageTable.offset += payloadLen_;
        // check for incomplete section
        if( package_->packageTable.offset < package_->packageTable.len )
            return AVCONTEXT_CONTINUE;
    }

    // now entire table is filled
    const quint8* psi = package_->packageTable.buf;
    const quint8* endPsi = psi + package_->packageTable.len;

    switch( package_->packageTable.tableId )
    {
        case 0x00: // parse PAT table
        {
            // check if version number changed
            quint16 id = avRb16(psi);
            // check if applicable
            if( (avRb8(psi+2) & 0x01) == 0)
                return AVCONTEXT_CONTINUE;
            // check if version number changed
            quint8 version = (avRb8(psi+2) & 0x3e) >> 1;
            if( id == package_->packageTable.id && 
                version == package_->packageTable.version )
            {
                return AVCONTEXT_CONTINUE;
            }

            // clear old associated pmt
            clearPmt();
            // parse new version of PAT
            psi += 5;
            // CRC32
            endPsi -= 4; 

            if( psi >= endPsi )
                return AVCONTEXT_TS_ERROR;

            if( (len = endPsi - psi) % 4 )
                return AVCONTEXT_TS_ERROR;

            qint32 n = len / 4;
            for( qint32 i = 0; i < n; i++, psi += 4)
            {
                quint16 channel = avRb16(psi);
                quint16 pmtPid  = avRb16(psi+2);

                // Reserved fields in table sections must be "set" to '1' bits.
                //if( (pmtPid & 0xe000) != 0xe000 )
                //    return AVCONTEXT_TS_ERROR;
                pmtPid &= 0x1fff;
                if( channel_ == 0 || channel_ == channel)
                {
                    TsPackage& pmt = packages_[pmtPid];
                    pmt.pid = pmtPid;
                    pmt.packageType = PACKAGE_TYPE_PSI;
                    pmt.channel = channel;
                }
            }

            // PAT is processed. New version is available
            package_->packageTable.id = id;
            package_->packageTable.version = version;
            break;
        }
        case 0x02: // parse PMT table
        {
            quint16 id = avRb16(psi);
            // check if applicable
            if( (avRb8(psi+2) & 0x01) == 0 )
                return AVCONTEXT_CONTINUE;
            // check if version number changed
            quint8 version = (avRb8(psi+2) & 0x3e) >> 1;
            if( id == package_->packageTable.id && 
                version == package_->packageTable.version)
            {
                return AVCONTEXT_CONTINUE;
            }

            // clear old pes
            clearPes(package_->channel);

            // parse new version of PMT
            psi += 7;
            endPsi -= 4; // CRC32
            if( psi >= endPsi )
                return AVCONTEXT_TS_ERROR;

            len = (qint32)(avRb16(psi) & 0x0fff);
            psi += 2 + len;

            while( psi < endPsi )
            {
                if( endPsi - psi < 5 )
                    return AVCONTEXT_TS_ERROR;

                quint8  pesType = avRb8(psi);
                quint16 pesPid  = avRb16(psi+1);

                // Reserved fields in table sections must be "set" to '1' bits.
                //if( (pesPid & 0xe000) != 0xe000 )
                //    return AVCONTEXT_TS_ERROR;

                pesPid &= 0x1fff;

                // len of descriptor section
                len = (qint32)(avRb16(psi+3) & 0x0fff);
                psi += 5;

                // ignore unknown streams
                STREAM_TYPE streamType = getStreamType(pesType);
                if( streamType != STREAM_TYPE_UNKNOWN )
                {
                    TsPackage& pes = packages_[pesPid];
                    pes.pid = pesPid;
                    pes.packageType = PACKAGE_TYPE_PES;
                    pes.channel = package_->channel;
                    // disable streaming by default
                    pes.streaming = false;

                    // get basic stream infos from PMT table
                    STREAM_INFO streamInfo = parsePesDescriptor(psi, len, &streamType);
                    TsStream* es;
                    switch( streamType )
                    {
                    case STREAM_TYPE_VIDEO_MPEG1:
                    case STREAM_TYPE_VIDEO_MPEG2:
                        es = new MPEG2Video(pesPid);
                        break;
                    case STREAM_TYPE_AUDIO_MPEG1:
                    case STREAM_TYPE_AUDIO_MPEG2:
                        es = new MPEG2Audio(pesPid);
                        break;
                    case STREAM_TYPE_AUDIO_AAC:
                    case STREAM_TYPE_AUDIO_AAC_ADTS:
                    case STREAM_TYPE_AUDIO_AAC_LATM:
                        es = new AAC(pesPid);
                        break;
                    case STREAM_TYPE_VIDEO_H264:
                        es = new h264(pesPid);
                        break;
                    case STREAM_TYPE_AUDIO_AC3:
                    case STREAM_TYPE_AUDIO_EAC3:
                        es = new AC3(pesPid);
                        break;
                    case STREAM_TYPE_DVB_SUBTITLE:
                        es = new Subtitle(pesPid);
                        break;
                    case STREAM_TYPE_DVB_TELETEXT:
                        es = new Teletext(pesPid);
                        break;
                    default:
                        // No parser: pass-through
                        es = new TsStream(pesPid);
                        es->hasStreamInfo_ = true;
                        break;
                    }

                    es->streamType_ = streamType;
                    es->streamInfo_ = streamInfo;
                    pes.pStream = es;
                }
                psi += len;
            }

            if( psi != endPsi )
                return AVCONTEXT_TS_ERROR;

            // PMT is processed. New version is available
            package_->packageTable.id = id;
            package_->packageTable.version = version;
            return AVCONTEXT_PROGRAM_CHANGE;
        }
        default:
            // CAT, NIT table
            break;
    }
   
    return AVCONTEXT_CONTINUE;
}

STREAM_INFO AVContext::parsePesDescriptor(const quint8* p, qint32 len, STREAM_TYPE* st)
{
    const quint8* descEnd = p + len;
    STREAM_INFO si;
    memset(&si, 0, sizeof(STREAM_INFO));

    while( p < descEnd )
    {
        quint8 descTag = avRb8(p);
        quint8 descLen = avRb8(p+1);
        p += 2;

        switch( descTag )
        {
        case 0x02:
        case 0x03:
            break;
        case 0x0a: // ISO 639 language descriptor
            if( descLen >= 4 )
            {
                si.language[0] = avRb8(p);
                si.language[1] = avRb8(p+1);
                si.language[2] = avRb8(p+2);
                si.language[3] = 0;
            }
            break;

        case 0x56: // DVB teletext descriptor
            *st = STREAM_TYPE_DVB_TELETEXT;
            break;
        case 0x59: // subtitling descriptor
            if( descLen >= 8 )
            {
                // Byte 4 is the subtitling_type field
                // av_rb8(p + 3) & 0x10 : normal
                // av_rb8(p + 3) & 0x20 : for the hard of hearing
                *st = STREAM_TYPE_DVB_SUBTITLE;
                si.language[0] = avRb8(p);
                si.language[1] = avRb8(p+1);
                si.language[2] = avRb8(p+2);
                si.language[3] = 0;
                si.compositionId = (qint32)avRb16(p+4);
                si.ancillaryId = (qint32)avRb16(p+6);
            }
            break;
        case 0x6a: // DVB AC3
        case 0x81: // AC3 audio stream
            *st = STREAM_TYPE_AUDIO_AC3;
            break;
        case 0x7a: // DVB enhanced AC3
            *st = STREAM_TYPE_AUDIO_EAC3;
            break;
        case 0x7b: // DVB DTS
            *st = STREAM_TYPE_AUDIO_DTS;
            break;
        case 0x7c: // DVB AAC
            *st = STREAM_TYPE_AUDIO_AAC;
            break;
        case 0x05: // registration descriptor
        case 0x1E: // SL descriptor
        case 0x1F: // FMC descriptor
        case 0x52: // stream identifier descriptor
        default:
            break;
        }
        p += descLen;
    }
    return si;
}

// Parse PES payload
// returns:
// AVCONTEXT_CONTINUE
// Parse completed. When streaming is enabled for PID, data is appended to
// the frame buffer of corresponding elementary stream.
// AVCONTEXT_TS_ERROR
// Parsing error
qint32 AVContext::parseTsPes()
{
    if( !hasPayload_ || payload_ == NULL || payloadLen_ == 0 || package_ == NULL )
        return AVCONTEXT_CONTINUE;

    if( package_->pStream == NULL )
        return AVCONTEXT_CONTINUE;

    if( payloadUnitStart_ )
    {
        // wait for unit start: Reset frame buffer to clear old data
        if( this->package_->waitUnitStart )
        {
            package_->pStream->reset();
            package_->pStream->prevDts_ = PTS_UNSET;
            package_->pStream->prevPts_ = PTS_UNSET;
        }
        package_->waitUnitStart = false;
        package_->hasStreamData = false;
        // reset header table
        package_->packageTable.reset();
        // header len is at least 6 bytes. So getting 6 bytes first
        package_->packageTable.len = 6;
    }

    // position in the payload buffer. Start at 0
    qint32 pos = 0;
    while( package_->packageTable.offset < package_->packageTable.len )
    {
        if( pos >= payloadLen_ )
            return AVCONTEXT_CONTINUE;

        qint32 n = package_->packageTable.len - package_->packageTable.offset;

        if( n > payloadLen_ - pos )
            n = payloadLen_ - pos;

        memcpy(package_->packageTable.buf + package_->packageTable.offset, payload_+pos, n);
        package_->packageTable.offset += n;
        pos += n;

        if( package_->packageTable.offset == 6 )
        {
            if( memcmp(package_->packageTable.buf, "\x00\x00\x01", 3) == 0 )
            {
                quint8 streamId = avRb8(package_->packageTable.buf+3);
                if( streamId == 0xbd || (streamId >= 0xc0 && streamId <= 0xef) )
                    package_->packageTable.len = 9;
            }
        }
        else if( package_->packageTable.offset == 9 )
            package_->packageTable.len += avRb8(package_->packageTable.buf+8);
    }

    // parse header table
    bool hasPts = false;

    if( package_->packageTable.len >= 9 )
    {
        quint8 flags = avRb8(package_->packageTable.buf+7);

        //package_->pStream->frame_num++;
        switch (flags & 0xc0) {
        case 0x80: // PTS only
            hasPts = true;
            if( package_->packageTable.len >= 14 )
            {
                qint64 pts = decodePts(package_->packageTable.buf+9);
                package_->pStream->prevDts_ = package_->pStream->curDts_;
                package_->pStream->prevPts_ = package_->pStream->curPts_;
                package_->pStream->curDts_  = package_->pStream->curPts_ = pts;
            }
            else
                package_->pStream->curDts_ = package_->pStream->curPts_ = PTS_UNSET;
            break;
        case 0xc0: // PTS,DTS
            hasPts = true;
            if( package_->packageTable.len >= 19 )
            {
                qint64 pts = decodePts(package_->packageTable.buf+9);
                qint64 dts = decodePts(package_->packageTable.buf+14);
                qint64 d = (pts - dts) & PTS_MASK;
                // more than two seconds of PTS/DTS delta, probably corrupt
                if(d > 180000)
                {
                    package_->pStream->curDts_ = package_->pStream->curPts_ = PTS_UNSET;
                }
                else
                {
                    package_->pStream->prevDts_ = package_->pStream->curDts_;
                    package_->pStream->prevPts_ = package_->pStream->curPts_;
                    package_->pStream->curDts_  = dts;
                    package_->pStream->curPts_  = pts;
                }
            }
            else
            {
                package_->pStream->curDts_ = package_->pStream->curPts_ = PTS_UNSET;
            }
            break;
        }
        package_->packageTable.reset();
    }

    if( package_->streaming )
    {
        const quint8* data = payload_ + pos;
        qint32 len = payloadLen_ - pos;
        package_->pStream->append(data, len, hasPts);
    }
    return AVCONTEXT_CONTINUE;
}
