#include "ts_aac.h"
#include "bitstream.h"

static qint32 aacSampleRates[16] =
{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};


AAC::AAC(quint16 pesPid)
    : TsStream(pesPid)
{
    configured_         = false;
    frameLengthType_    = 0;
    PTS_                = 0;
    DTS_                = 0;
    frameSize_          = 0;
    sampleRate_         = 0;
    channels_           = 0;
    bitRate_            = 0;
    audioMuxVersion_A   = 0;
    esAllocInit_        = 1920*2;
    reset();
}

AAC::~AAC()
{}

void AAC::parse(STREAM_PKG* pkg)
{
    qint32 p = esParsed_, c;
    while( (c = esLen_ - p) > 8 )
    {
        if( findHeaders(esBuf_ + p, c) < 0 )
            break;
        p++;
    }
    esParsed_ = p;

    if( esFoundFrame_ && c >= frameSize_ )
    {
        bool streamChange = setAudioInformation(channels_, sampleRate_, bitRate_, 0, 0);
        pkg->pid            = pid_;
        pkg->data           = &esBuf_[p];
        pkg->size           = frameSize_;
        pkg->duration       = 1024 * 90000 / sampleRate_;
        pkg->dts            = DTS_;
        pkg->pts            = PTS_;
        pkg->streamChange   = streamChange;

        esConsumed_ = p + frameSize_;
        esParsed_ = esConsumed_;
        esFoundFrame_ = false;
    }
}

qint32 AAC::findHeaders(quint8* buf, qint32 bufSize)
{
    if( esFoundFrame_ )
        return -1;

    quint8* bufPtr = buf;

    if( streamType_ == STREAM_TYPE_AUDIO_AAC )
    {
        if( bufPtr[0] == 0xFF && (bufPtr[1] & 0xF0) == 0xF0 )
            streamType_ = STREAM_TYPE_AUDIO_AAC_ADTS;
        else if( bufPtr[0] == 0x56 && (bufPtr[1] & 0xE0) == 0xE0 )
            streamType_ = STREAM_TYPE_AUDIO_AAC_LATM;
    }

    // STREAM_TYPE_AUDIO_AAC_LATM
    if( streamType_ == STREAM_TYPE_AUDIO_AAC_LATM )
    {
        if( (bufPtr[0] == 0x56 && (bufPtr[1] & 0xE0) == 0xE0) )
        {
            // TODO
            if( bufSize < 16)
                return -1;

            BitStream bs(bufPtr, 16 * 8);
            bs.skipBits(11);
            frameSize_ = bs.readBits(13) + 3;
            if( !parseLATMAudioMuxElement(&bs) )
                return 0;

            esFoundFrame_ = true;
            DTS_ = curPts_;
            PTS_ = curPts_;
            curPts_ += 90000 * 1024 / sampleRate_;
            return -1;
        }
    }
    // STREAM_TYPE_AUDIO_AAC_ADTS
    else if( streamType_ == STREAM_TYPE_AUDIO_AAC_ADTS )
    {
        if( bufPtr[0] == 0xFF && (bufPtr[1] & 0xF0) == 0xF0 )
        {
            // need at least 7 bytes for header
            if( bufSize < 7 )
                return -1;

            BitStream bs(bufPtr, 9 * 8);
            bs.skipBits(15);

            // check if CRC is present, means header is 9 byte long
            qint32 noCrc = bs.readBits(1);
            if( noCrc == 0 && bufSize < 9 )
                return -1;

            bs.skipBits(2); // profile
            qint32 sampleRateIndex = bs.readBits(4);
            bs.skipBits(1); // private
            channels_ = bs.readBits(3);
            bs.skipBits(4);

            frameSize_  = bs.readBits(13);
            sampleRate_ = aacSampleRates[sampleRateIndex & 0x0E];

            esFoundFrame_ = true;
            DTS_ = curPts_;
            PTS_ = curPts_;
            curPts_ += 90000 * 1024 / sampleRate_;
            return -1;
        }
    }
    return 0;
}

bool AAC::parseLATMAudioMuxElement(BitStream *bs)
{
    if( 0 == bs->readBits1() )
        readStreamMuxConfig(bs);
    return configured_;
}

void AAC::readStreamMuxConfig(BitStream *bs)
{
    qint32 audioMuxVersion = bs->readBits(1);
    audioMuxVersion_A = 0;
    if( audioMuxVersion != 0 )
        audioMuxVersion_A = bs->readBits(1);

    if( audioMuxVersion_A != 0 )
        return;

    if( audioMuxVersion != 0 )
        LATMGetValue(bs);                    // taraFullness

    bs->skipBits(1);                         // allStreamSameTimeFraming = 1
    bs->skipBits(6);                         // numSubFrames = 0
    bs->skipBits(4);                         // numPrograms = 0

    // for each program (which there is only on in DVB)
    bs->skipBits(3);                         // numLayer = 0

    // for each layer (which there is only on in DVB)
    if( audioMuxVersion == 0 )
    {
        readAudioSpecificConfig(bs);
        if( sampleRate_ == 0 )
            return;
    }
    else
        return;

    // these are not needed... perhaps
    frameLengthType_ = bs->readBits(3);
    switch( frameLengthType_ )
    {
    case 0:
        bs->readBits(8);
        break;
    case 1:
        bs->readBits(9);
        break;
    case 3:
    case 4:
    case 5:
        bs->readBits(6);    // celp_table_index
        break;
    case 6:
    case 7:
        bs->readBits(1);    // hvxc_table_index
        break;
    }

    if( bs->readBits(1) ) { // other data?
        qint32 esc;
        do {
            esc = bs->readBits(1);
            bs->skipBits(8);
        } 
        while( esc );
    }

    if( bs->readBits(1) )    // crc present?
        bs->skipBits(8);     // config_crc
    configured_ = true;
}

void AAC::readAudioSpecificConfig(BitStream *bs)
{
    qint32 aot = bs->readBits(5);
    if( aot == 31 )
        aot = 32 + bs->readBits(6);

    qint32 sampleRateIndex = bs->readBits(4);

    if( sampleRateIndex == 0xf )
        sampleRate_ = bs->readBits(24);
    else
        sampleRate_ = aacSampleRates[sampleRateIndex & 0xf];

    channels_ = bs->readBits(4);

    if( aot == 5 ) { // AOT_SBR
        if( bs->readBits(4) == 0xf ) // extensionSamplingFrequencyIndex
            bs->skipBits(24);
        aot = bs->readBits(5); // this is the main object type (i.e. non-extended)
        if( aot == 31 )
            aot = 32 + bs->readBits(6);
    }

    if( aot != 2 )
        return;

    bs->skipBits(1);        //framelen_flag
    if( bs->readBits1() )   // depends_on_coder
        bs->skipBits(14);

    if( bs->readBits(1) )   // ext_flag
        bs->skipBits(1);    // ext3_flag
}

void AAC::reset()
{
    TsStream::reset();
    configured_ = false;
}
