#ifndef __ts_h264_h__
#define __ts_h264_h__

#include "tsstream.h"

class h264 : public TsStream
{
private:
    struct h264_private
    {
        struct SPS
        {
            qint32 frameDuration;
            qint32 cbpSize;
            qint32 picOrderCntType;
            qint32 frameMbsOnlyFlag;
            qint32 log2MaxFrameNum;
            qint32 log2MaxPicOrderCntLsb;
            qint32 deltaPicOrderAlwaysZeroFlag;
        } sps[256];

        struct PPS
        {
            qint32 sps;
            qint32 picOrderPresentFlag;
        } pps[256];

        struct VCL_NAL
        {
            qint32 frameNum;                // slice
            qint32 picParameterSetId;       // slice
            qint32 fieldPicFlag;            // slice
            qint32 bottomFieldFlag;         // slice
            qint32 deltaPicOrderCntBottom;  // slice
            qint32 deltaPicOrderCnt0;       // slice
            qint32 deltaPicOrderCnt1;       // slice
            qint32 picOrderCntLsb;          // slice
            qint32 idrPicId;                // slice
            qint32 nalUnitType;
            qint32 nalRefIdc;               // start code
            qint32 picOrderCntType;         // sps
        } vcl_nal;
    };

    struct MpegRational {
        qint32 num;
        qint32 den;
    };

    enum
    {
        NAL_SLH     = 0x01, // slice header
        NAL_SEI     = 0x06, // supplemental enhancement information
        NAL_SPS     = 0x07, // sequence parameter set
        NAL_PPS     = 0x08, // picture parameter set
        NAL_AUD     = 0x09, // access unit delimiter
        NAL_END_SEQ = 0x0A  // end of sequence
    };

    quint32         startCode_;
    bool            needIFrame_;
    bool            needSPS_;
    bool            needPPS_;
    qint32          width_;
    qint32          height_;
    qint32          FPS_;
    qint32          fpsScale_;
    MpegRational    pixelAspect_;
    qint32          frameDuration_;
    h264_private    streamData_;
    qint32          vbvDelay_;       // -1 if CBR
    qint32          vbvSize_;        // Video buffer size (in bytes)
    qint64          DTS_;
    qint64          PTS_;
    bool            interlaced_;

    qint32  parse_H264(quint32 startcode, qint32 bufPtr, bool& complete);
    bool    parse_PPS(quint8* buf, qint32 len);
    bool    parse_SLH(quint8* buf, qint32 len, h264_private::VCL_NAL& vcl);
    bool    parse_SPS(quint8* buf, qint32 len);
    bool    isFirstVclNal(h264_private::VCL_NAL& vcl);

public:
    h264(quint16 pesPid);
    virtual ~h264();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};


#endif // __ts_h264_h__
