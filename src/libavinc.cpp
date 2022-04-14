#ifndef LIBAVINC_CPP
#define LIBAVINC_CPP

#include "libavinc.hpp"

extern "C" {
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace libav {

::AVDictionary* av_dictionary(const libav::AVDictionary& dict)
{
    ::AVDictionary* d = nullptr;
    for (const auto& entry : dict) {
        av_dict_set(&d, entry.first.c_str(), entry.second.c_str(), 0);
    }
    return d;
}

libav::AVDictionary av_dictionary(const ::AVDictionary* dict)
{
    libav::AVDictionary d;
    AVDictionaryEntry* entry = nullptr;
    while ((entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX))) {
        d.emplace(entry->key, entry->value);
    }
    return d;
}

void av_dict_free(::AVDictionary* d)
{
    if (d) {
        av_dict_free(&d);
    }
}

///////////////////////////////////////////////////////////////////////////////


int av_read_frame(::AVFormatContext* ctx, ::AVPacket* pkt)
{
    if (!ctx || !pkt) {
        return AVERROR(1);
    }

    auto err = ::av_read_frame(ctx, pkt);
    if (0 <= err) {
        auto& track = ctx->streams[pkt->stream_index];
        ::av_packet_rescale_ts(pkt, track->time_base, FLICKS_TIMESCALE_Q);
        // TODO check for pkt->size == 0 but not EOF
    } else {
        pkt = ::av_packet_alloc();
        pkt->size = 0;
        pkt->data = nullptr;
    }

    return err;
}

AVFormatContext avformat_open_input(const std::string& url, const libav::AVDictionary& options)
{
    ::AVFormatContext* fmtCtx = nullptr;
    auto avdict = libav::av_dictionary(options);
    auto err = ::avformat_open_input(&fmtCtx, url.c_str(), nullptr, &avdict);
    libav::av_dict_free(avdict);
    if (err < 0 || !fmtCtx) {
        return libav::AVFormatContext();
    }

    err = ::avformat_find_stream_info(fmtCtx, nullptr);
    if (err < 0) {
        ::avformat_close_input(&fmtCtx);
        return libav::AVFormatContext();
    }

    return libav::AVFormatContext(fmtCtx, [](::AVFormatContext* ctx) {
        auto p_ctx = &ctx;
        ::avformat_close_input(p_ctx);
    });
}

// Modified from instructions at https://habr.com/en/company/intel/blog/575632/
AVCodecContext make_encode_context(std::string codec_name,int width, int height, int fps, ::AVPixelFormat pix_fmt)
{
    const ::AVCodec* codec = ::avcodec_find_encoder_by_name(codec_name.c_str());
    auto codecCtx = AVCodecContext(::avcodec_alloc_context3(codec),
        [](::AVCodecContext* c) {
            ::avcodec_free_context(&c);
        });

    codecCtx->width = width;
    codecCtx->height = height;
    codecCtx->time_base = ::AVRational({1,fps});
    codecCtx->framerate = ::AVRational({fps,1});
    codecCtx->sample_aspect_ratio = ::AVRational({1,1});
    codecCtx->pix_fmt = pix_fmt;

    return codecCtx;
}

AVCodecContext make_encode_context_nvenc(int width, int height, int fps) {
    return make_encode_context("h264_nvenc",width,height,fps,AV_PIX_FMT_CUDA);
}


} // End libav namespace
#endif // LIBAVINC_CPP
