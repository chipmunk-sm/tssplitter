#include "ts_h264.h"
#include "bitstream.h"

static const qint32 h264_lev2cpbsize[][2] =
{
    {10, 175},
    {11, 500},
    {12, 1000},
    {13, 2000},
    {20, 2000},
    {21, 4000},
    {22, 4000},
    {30, 10000},
    {31, 14000},
    {32, 20000},
    {40, 25000},
    {41, 62500},
    {42, 62500},
    {50, 135000},
    {51, 240000},
    {-1, -1},
};

h264::h264(quint16 pesPid)
    : TsStream(pesPid)
{
    height_             = 0;
    width_              = 0;
    FPS_                = 25;
    fpsScale_           = 0;
    frameDuration_      = 0;
    vbvDelay_           = -1;
    vbvSize_            = 0;
    pixelAspect_.den    = 1;
    pixelAspect_.num    = 0;
    DTS_                = 0;
    PTS_                = 0;
    interlaced_         = false;
    esAllocInit_        = 240000;
    reset();
}

h264::~h264()
{}

void h264::parse(STREAM_PKG* pkg)
{
    qint32  frame_ptr = esConsumed_, p = esParsed_, c;
    quint32 startcode = startCode_;
    bool frameComplete = false;
  
    while( (c = esLen_ - p) > 3 ) {
        if( (startcode & 0xffffff00) == 0x00000100 )
            if( parse_H264(startcode, p, frameComplete) < 0 )
                break;
        startcode = startcode << 8 | esBuf_[p++];
    }
    esParsed_ = p;
    startCode_ = startcode;

    if( frameComplete )
    {
        if( !needSPS_ && !needIFrame_ )
        {
            qreal PAR = (qreal)pixelAspect_.num/(qreal)pixelAspect_.den;
            qreal DAR = (PAR * width_) / height_;
            if( fpsScale_ == 0 )
            {
                fpsScale_ = static_cast<qint32>(rescale(curDts_ - prevDts_, RESCALE_TIME_BASE, PTS_TIME_BASE));
            }
      
            bool streamChange = setVideoInformation(fpsScale_, RESCALE_TIME_BASE, height_, width_, static_cast<float>(DAR), interlaced_);
            pkg->pid            = pid_;
            pkg->size           = esConsumed_ - frame_ptr;
            pkg->data           = &esBuf_[frame_ptr];
            pkg->dts            = DTS_;
            pkg->pts            = PTS_;
            pkg->duration       = curDts_ - prevDts_;
            pkg->streamChange   = streamChange;
        }
        startCode_ = 0xffffffff;
        esParsed_ = esConsumed_;
        esFoundFrame_ = false;
    }
}

void h264::reset()
{
    TsStream::reset();
    startCode_  = 0xffffffff;
    needIFrame_ = true;
    needSPS_    = true;
    needPPS_    = true;
    memset(&streamData_, 0, sizeof(streamData_));
}

qint32 h264::parse_H264(quint32 startcode, qint32 bufPtr, bool& complete)
{
    qint32  len = esLen_ - bufPtr;
    quint8* buf = esBuf_ + bufPtr;

    switch(startcode & 0x9f)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        if( needSPS_ || needPPS_ ) {
            esFoundFrame_ = true;
            return 0;
        }

        // need at least 32 bytes for parsing nal
        if( len < 32 )
            return -1;

        h264_private::VCL_NAL vcl;
        memset(&vcl, 0, sizeof(h264_private::VCL_NAL));
        vcl.nalRefIdc = startcode & 0x60;
        vcl.nalUnitType = startcode & 0x1F;
        if( !parse_SLH(buf, len, vcl) )
            return 0;

        // check for the beginning of a new access unit
        if( esFoundFrame_ && isFirstVclNal(vcl) ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }

        if( !esFoundFrame_ )
        {
            if (bufPtr - 4 >= (int)esPtsPointer_) {
                DTS_ = curDts_;
                PTS_ = curPts_;
            }
            else {
                DTS_ = prevDts_;
                PTS_ = prevPts_;
            }
        }

        streamData_.vcl_nal = vcl;
        esFoundFrame_ = true;
        break;

    case NAL_SEI:
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }
        break;

    case NAL_SPS:
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }
        // TODO: how big is SPS?
        if( len < 256 )
            return -1;
        if( !parse_SPS(buf, len) )
            return 0;

        needSPS_ = false;
        break;

    case NAL_PPS:
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }
        // TODO: how big is PPS
        if( len < 64 )
            return -1;

        if( !parse_PPS(buf, len) )
            return 0;

        needPPS_ = false;
        break;

    case NAL_AUD:
        if( esFoundFrame_ && (prevDts_ != PTS_UNSET) ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }
        break;

    case NAL_END_SEQ:
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr;
            return -1;
        }
        break;

    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }
        break;

    default:
        break;
    }
    return 0;
}

bool h264::parse_PPS(quint8* buf, qint32 len)
{
    BitStream bs(buf, len*8);

    qint32 ppsId = bs.readGolombUE();
    qint32 spsId = bs.readGolombUE();
    streamData_.pps[ppsId].sps = spsId;
    bs.readBits1();
    streamData_.pps[ppsId].picOrderPresentFlag = bs.readBits1();
    return true;
}

bool h264::parse_SLH(quint8* buf, qint32 len, h264_private::VCL_NAL& vcl)
{
    BitStream bs(buf, len*8);

    bs.readGolombUE();      // first_mb_in_slice
    qint32 slice_type = bs.readGolombUE();

    if (slice_type > 4)
        slice_type -= 5;    // Fixed slice type per frame */

    switch(slice_type)
    {
    case 0:
    case 1:
        break;
    case 2:
        needIFrame_ = false;
        break;
    default:
        return false;
    }

    qint32 ppsId = bs.readGolombUE();
    qint32 spsId = streamData_.pps[ppsId].sps;
    if( streamData_.sps[spsId].cbpSize == 0 )
        return false;

    vbvSize_  = streamData_.sps[spsId].cbpSize;
    vbvDelay_ = -1;

    vcl.picParameterSetId = ppsId;
    vcl.frameNum = bs.readBits(streamData_.sps[spsId].log2MaxFrameNum);
    if( streamData_.sps[spsId].frameMbsOnlyFlag == 0 )
    {
        vcl.fieldPicFlag = bs.readBits1();
        // interlaced
        if( vcl.fieldPicFlag != 0 )
            interlaced_ = true;
    }

    if( vcl.fieldPicFlag != 0 )
        vcl.bottomFieldFlag = bs.readBits1();

    if( vcl.nalUnitType == 5 )
        vcl.idrPicId = bs.readGolombUE();

    if( streamData_.sps[spsId].picOrderCntType == 0 )
    {
        vcl.picOrderCntLsb = bs.readBits(streamData_.sps[spsId].log2MaxPicOrderCntLsb);
        if( streamData_.pps[ppsId].picOrderPresentFlag != 0 && vcl.fieldPicFlag == 0 )
            vcl.deltaPicOrderCntBottom = bs.readGolombSE();
    }

    if( streamData_.sps[spsId].picOrderCntType == 1 &&
        streamData_.sps[spsId].deltaPicOrderAlwaysZeroFlag == 0 )
    {
        vcl.deltaPicOrderCnt0 = bs.readGolombSE();
        if( streamData_.pps[ppsId].picOrderPresentFlag != 0 && vcl.fieldPicFlag == 0 )
            vcl.deltaPicOrderCnt1 = bs.readGolombSE();
    }

    vcl.picOrderCntType = streamData_.sps[spsId].picOrderCntType;
    return true;
}

bool h264::parse_SPS(quint8* buf, qint32 len)
{
    BitStream bs(buf, len*8);
    quint32 tmp, frameMbsOnly;
    qint32  cbpSize = -1;

    qint32 profileIdc = bs.readBits(8);
    // constraint_set0_flag = bs.readBits1();
    // constraint_set1_flag = bs.readBits1();
    // constraint_set2_flag = bs.readBits1();
    // constraint_set3_flag = bs.readBits1();
    // reserved             = bs.readBits(4);
    bs.skipBits(8);
    qint32  levelIdc = bs.readBits(8);
    quint32 seqParameterSetId = bs.readGolombUE(9);

    quint32 i = 0;
    while( h264_lev2cpbsize[i][0] != -1 )
    {
        if( h264_lev2cpbsize[i][0] >= levelIdc ) {
            cbpSize = h264_lev2cpbsize[i][1];
            break;
        }
        i++;
    }

    if( cbpSize < 0 )
        return false;

    memset(&streamData_.sps[seqParameterSetId], 0, sizeof(h264_private::SPS));
    streamData_.sps[seqParameterSetId].cbpSize = cbpSize * 125; // Convert from kbit to bytes

    if( profileIdc == 100 || profileIdc == 110 ||
        profileIdc == 122 || profileIdc == 244 || profileIdc == 44  ||
        profileIdc == 83  || profileIdc == 86  || profileIdc == 118 ||
        profileIdc == 128 )
    {
        qint32 chromaFormatIdc = bs.readGolombUE(9);    // chromaFormatIdc
        if( chromaFormatIdc == 3 )
            bs.skipBits(1);             // residualColourTransformFlag
        bs.readGolombUE();              // bitDepthLuma - 8
        bs.readGolombUE();              // bitDepthChroma - 8
        bs.skipBits(1);                 // transformBypass

        if( bs.readBits1() != 0 )            // seqScalingMatrixPresent
        {
            for( qint32 i = 0; i < ((chromaFormatIdc != 3) ? 8 : 12); i++ )
            {
                if( bs.readBits1() != 0 )    // seqScalingListPresent
                {
                    qint32 last = 8, next = 8, size = (i<6) ? 16 : 64;
                    for( qint32 j = 0; j < size; j++ )
                    {
                        if( next != 0 )
                            next = (last + bs.readGolombSE()) & 0xff;
                        last = !next ? last: next;
                    }
                }
            }
        }
    }

    qint32 log2MaxFrameNumMinus4 = bs.readGolombUE(); // log2MaxFrameNum - 4
    streamData_.sps[seqParameterSetId].log2MaxFrameNum = log2MaxFrameNumMinus4 + 4;

    qint32 picOrderCntType = bs.readGolombUE(9);
    streamData_.sps[seqParameterSetId].picOrderCntType = picOrderCntType;
    if( picOrderCntType == 0 )
    {
        qint32 log2MaxPicOrderCntLsbMinus4 = bs.readGolombUE(); // log2MaxPocLsb - 4
        streamData_.sps[seqParameterSetId].log2MaxPicOrderCntLsb = log2MaxPicOrderCntLsbMinus4 + 4;
    }
    else if( picOrderCntType == 1 )
    {
        streamData_.sps[seqParameterSetId].deltaPicOrderAlwaysZeroFlag = bs.readBits1();
        bs.readGolombSE();              // offsetForNonRefPic
        bs.readGolombSE();              // offsetForTopToBottomField
        tmp = bs.readGolombUE();        // numRefFramesInPicOrderCntCycle
        for(quint32 i = 0; i < tmp; i++)
            bs.readGolombSE();          // offsetForRefFrame[i]
    }
    else if( picOrderCntType != 2 )
    {
        // Illegal poc
        return false;
    }

    bs.readGolombUE(9);         // refFrames
    bs.skipBits(1);             // gapsInFrameNumAllowed
    width_  /* mbs */ = bs.readGolombUE() + 1;
    height_ /* mbs */ = bs.readGolombUE() + 1;
    frameMbsOnly      = bs.readBits1();
    streamData_.sps[seqParameterSetId].frameMbsOnlyFlag = frameMbsOnly;

    width_  *= 16;
    height_ *= 16 * (2-frameMbsOnly);

    if( frameMbsOnly == 0 )
        if( bs.readBits1() != 0 )     // mbAdaptiveFrameFieldFlag
        {
            // qDebug() << DBG_PARSE << "H.264 SPS: MBAFF";
        }

    bs.skipBits(1);            // direct8x8InferenceFlag
    if( bs.readBits1() != 0 )       // frameCroppingFlag
    {
        quint32 cropLeft   = bs.readGolombUE();
        quint32 cropRight  = bs.readGolombUE();
        quint32 cropTop    = bs.readGolombUE();
        quint32 cropBottom = bs.readGolombUE();
        
        width_ -= 2*(cropLeft + cropRight);
        if( frameMbsOnly != 0 )
            height_ -= 2*(cropTop + cropBottom);
        else
            height_ -= 4*(cropTop + cropBottom);
    }

    // VUI parameters
    pixelAspect_.num = 0;
    if( bs.readBits1() != 0 )       // vuiParametersPresent flag
    {
        if( bs.readBits1() != 0 )   // aspectRatioInfoPresent
        {
            quint32 aspectRatioIdc = bs.readBits(8);
            if( aspectRatioIdc == 255 /* Extended_SAR */)
            {
                pixelAspect_.num = bs.readBits(16); // sar_width
                pixelAspect_.den = bs.readBits(16); // sar_height
            }
            else
            {
                static const MpegRational aspectRatios[] =
                {   /* page 213: */
                    /* 0: unknown */
                    {0, 1},
                    /* 1...16: */
                    { 1,  1}, {12, 11}, {10, 11}, {16, 11}, { 40, 33}, {24, 11}, {20, 11}, {32, 11},
                    {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, { 4,  3}, { 3,  2}, { 2,  1}
                };

                if( aspectRatioIdc < sizeof(aspectRatios)/sizeof(aspectRatios[0]) )
                {
                    memcpy(&pixelAspect_, &aspectRatios[aspectRatioIdc], sizeof(MpegRational));
                }
                else
                {
                    //qDebug() << DBG_PARSE << "H.264 SPS: aspect_ratio_idc out of range !";
                }
            }
        }

        if( bs.readBits1() != 0 ) // overscan
        {
            bs.readBits1();       // overscanAppropriateFlag
        }

        if( bs.readBits1() != 0 ) // videoSignalTypePresentFlag
        {
            bs.readBits(3); // videoFormat
            bs.readBits1(); // videoFullRangeFlag
            if( bs.readBits1() != 0 ) // colourDescriptionPresentFlag
            {
                bs.readBits(8); // colourPrimaries
                bs.readBits(8); // transferCharacteristics
                bs.readBits(8); // matrixCoefficients
            }
        }

        if( bs.readBits1() != 0 ) // chromaLocInfoPresentFlag
        {
            bs.readGolombUE(); // chromaSampleLocTypeTopField
            bs.readGolombUE(); // chromaSampleLocTypeBottomField
        }

        if( bs.readBits1() != 0 ) // timing_info_present_flag
        {
//          quint32 numUnitsInTick = bs.readBits(32);
//          quint32 timeScale = bs.readBits(32);
//          int fixedFrameRate = bs.readBits1();
//          if( numUnitsInTick > 0 )
//              m_FPS = timeScale / (numUnitsInTick*2);
        }
    }
    return true;
}

bool h264::isFirstVclNal(h264_private::VCL_NAL& vcl)
{
    if( streamData_.vcl_nal.frameNum != vcl.frameNum )
        return true;

    if( streamData_.vcl_nal.picParameterSetId != vcl.picParameterSetId )
        return true;

    if( streamData_.vcl_nal.fieldPicFlag != vcl.fieldPicFlag )
        return true;

    if( streamData_.vcl_nal.fieldPicFlag && vcl.fieldPicFlag )
    {
        if( streamData_.vcl_nal.bottomFieldFlag != vcl.bottomFieldFlag )
            return true;
    }

    if( streamData_.vcl_nal.nalRefIdc == 0 || vcl.nalRefIdc == 0 )
    {
        if( streamData_.vcl_nal.nalRefIdc != vcl.nalRefIdc )
            return true;
    }

    if( streamData_.vcl_nal.picOrderCntType == 0 && vcl.picOrderCntType == 0 )
    {
        if( streamData_.vcl_nal.picOrderCntLsb != vcl.picOrderCntLsb )
            return true;
        if( streamData_.vcl_nal.deltaPicOrderCntBottom != vcl.deltaPicOrderCntBottom )
            return true;
    }

    if( streamData_.vcl_nal.picOrderCntType == 1 && vcl.picOrderCntType == 1 )
    {
        if( streamData_.vcl_nal.deltaPicOrderCnt0 != vcl.deltaPicOrderCnt0 )
            return true;
        if( streamData_.vcl_nal.deltaPicOrderCnt1 != vcl.deltaPicOrderCnt1 )
            return true;
    }

    if( streamData_.vcl_nal.nalUnitType == 5 || vcl.nalUnitType == 5 )
    {
        if( streamData_.vcl_nal.nalUnitType != vcl.nalUnitType )
            return true;
    }

    if( streamData_.vcl_nal.nalUnitType == 5 && vcl.nalUnitType == 5 )
    {
        if( streamData_.vcl_nal.idrPicId != vcl.idrPicId )
            return true;
    }
    return false;
}
