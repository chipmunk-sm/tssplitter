#ifndef TS_H264_H
#define TS_H264_H

#include "tsstream.h"

class h264 : public TsStream
{
private:
    struct h264_private
    {
        struct SPS
        {
            int32_t frameDuration;
            int32_t cbpSize;
            int32_t picOrderCntType;
            int32_t frameMbsOnlyFlag;
            int32_t log2MaxFrameNum;
            int32_t log2MaxPicOrderCntLsb;
            int32_t deltaPicOrderAlwaysZeroFlag;
        } sps[256];

        struct PPS
        {
            int32_t sps;
            int32_t picOrderPresentFlag;
        } pps[256];

        struct VCL_NAL
        {
            int32_t frameNum;                // slice
            int32_t picParameterSetId;       // slice
            int32_t fieldPicFlag;            // slice
            int32_t bottomFieldFlag;         // slice
            int32_t deltaPicOrderCntBottom;  // slice
            int32_t deltaPicOrderCnt0;       // slice
            int32_t deltaPicOrderCnt1;       // slice
            int32_t picOrderCntLsb;          // slice
            int32_t idrPicId;                // slice
            int32_t nalUnitType;
            int32_t nalRefIdc;               // start code
            int32_t picOrderCntType;         // sps
        } vcl_nal;
    };

    struct MpegRational
    {
        int32_t num;
        int32_t den;
    };

    enum
    {
        NAL_SLH = 0x01, // slice header
        NAL_SEI = 0x06, // supplemental enhancement information
        NAL_SPS = 0x07, // sequence parameter set
        NAL_PPS = 0x08, // picture parameter set
        NAL_AUD = 0x09, // access unit delimiter
        NAL_END_SEQ = 0x0A  // end of sequence
    };

    uint32_t         startCode_;
    bool            needIFrame_;
    bool            needSPS_;
    bool            needPPS_;
    int32_t          width_;
    int32_t          height_;
    double           fpsScale_;
    MpegRational     pixelAspect_;
    int32_t          frameDuration_;
    h264_private     streamData_;
    int32_t          vbvDelay_;       // -1 if CBR
    int32_t          vbvSize_;        // Video buffer size (in bytes)
    int64_t          DTS_;
    int64_t          PTS_;
    bool            interlaced_;

    int32_t  parse_H264(uint32_t startcode, int32_t bufPtr, bool& complete);
    bool    parse_PPS(uint8_t* buf, int32_t len);
    bool    parse_SLH(uint8_t* buf, int32_t len, h264_private::VCL_NAL& vcl);
    bool    parse_SPS(uint8_t* buf, int32_t len);
    bool    isFirstVclNal(h264_private::VCL_NAL& vcl);

public:
    h264(uint16_t pesPid);
    virtual ~h264();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};


#endif // TS_H264_H
