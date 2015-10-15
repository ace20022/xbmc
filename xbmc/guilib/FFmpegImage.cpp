/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */
#include "FFmpegImage.h"
#include "utils/log.h"
#include "cores/FFmpeg.h"
#include "guilib/Texture.h"

#include <algorithm>

extern "C"
{
#include <libavutil/imgutils.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
}

struct MemBuffer
{
  uint8_t* data;
  int64_t size;
  int64_t pos;
};

static int mem_file_read(void *h, uint8_t* buf, int size)
{
  MemBuffer* mbuf = static_cast<MemBuffer*>(h);
  int tocopy = (int) std::min((int64_t)size, mbuf->size-mbuf->pos);
  memcpy(buf, mbuf->data+mbuf->pos, tocopy);
  mbuf->pos += tocopy;
  return tocopy;
}

static int64_t mem_file_seek(void *h, int64_t pos, int whence)
{
  MemBuffer* mbuf = static_cast<MemBuffer*>(h);
  if(whence == AVSEEK_SIZE)
    return mbuf->size;

  whence &= ~AVSEEK_FORCE;
  if (whence == SEEK_SET)
    mbuf->pos = pos;
  else if (whence == SEEK_CUR)
    mbuf->pos = std::min(mbuf->pos+pos, mbuf->size-1);
  else
    mbuf->pos = mbuf->size+pos;

  return mbuf->pos;
}

CFFmpegImage::CFFmpegImage()
{
  m_hasAlpha = false;
  m_frame = av_frame_alloc();
}

CFFmpegImage::~CFFmpegImage()
{
}

bool CFFmpegImage::LoadImageFromMemory(unsigned char* buffer, unsigned int bufSize,
                                      unsigned int width, unsigned int height)
{
  uint8_t* fbuffer = (uint8_t*)av_malloc(32768);
  MemBuffer buf;
  buf.data = buffer;
  buf.size = bufSize;
  buf.pos = 0;

  AVIOContext* ioctx = avio_alloc_context(fbuffer, 32768, 0, &buf,
                                          mem_file_read, NULL, mem_file_seek);

  AVFormatContext* fctx = avformat_alloc_context();
  fctx->pb = ioctx;

  ioctx->max_packet_size = 32768;

  AVInputFormat* iformat = av_find_input_format("image");

  if (avformat_open_input(&fctx, "", iformat, NULL) < 0)
  {
    if (fctx)
      avformat_close_input(&fctx);
    av_free(ioctx->buffer);
    av_free(ioctx);
    return false;
  }
  AVCodecContext* codec_ctx = fctx->streams[0]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
  if (avcodec_open2(codec_ctx, codec, NULL) < 0)
  {
    if (fctx)
      avformat_close_input(&fctx);
    av_free(ioctx->buffer);
    av_free(ioctx);
    return false;
  }

  AVFrame* frame = av_frame_alloc();
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = nullptr;
  pkt.size = 0;

  av_read_frame(fctx, &pkt);
  int frame_decoded;
  int ret = avcodec_decode_video2(codec_ctx, frame, &frame_decoded, &pkt);
  if (ret < 0)
    CLog::Log(LOGDEBUG, "Error [%d] while decoding frame: %s\n", ret, strerror(AVERROR(ret)));

  if (frame_decoded)
  {
    CLog::Log(LOGDEBUG, "Frame decoded");
  }

  //int size = av_image_get_buffer_size((AVPixelFormat)m_frame->format, m_frame->width, m_frame->height, pitch);

  // Allocate the conversion frame and relevant picture
  m_pFrameRGB = (AVPicture*)av_mallocz(sizeof(AVPicture));

  if (!m_pFrameRGB)
    return false;

  m_height = frame->height;
  m_width = frame->width;

  // Due to a bug in swsscale we need to allocate one extra line of data
  if (avpicture_alloc(m_pFrameRGB, PIX_FMT_RGB32, m_width, m_height + 1) < 0)
    return false;

  // We got the video frame, render it into the picture buffer
  struct SwsContext * context = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
    m_width, m_height, PIX_FMT_RGB32, SWS_SPLINE, NULL, NULL, NULL);

  sws_scale(context, frame->data, frame->linesize, 0, codec_ctx->height,
    m_pFrameRGB->data, m_pFrameRGB->linesize);
  sws_freeContext(context);

  m_pitch = m_pFrameRGB->linesize[0];

  av_frame_free(&frame);
  av_free_packet(&pkt);
  avcodec_close(codec_ctx);
  avformat_close_input(&fctx);
  av_free(ioctx->buffer);
  av_free(ioctx);

  return true;
}

bool CFFmpegImage::Decode(unsigned char * const pixels, unsigned int pitch,
                          unsigned int format)
{
  if (m_width == 0 || m_height == 0 || format != XB_FMT_A8R8G8B8)
    return false;

  const unsigned char *src = m_pFrameRGB->data[0];
  unsigned char* dst = pixels;

  if (pitch == m_pitch)
    memcpy(dst, src, m_height * m_pitch);
  else
  {
    for (unsigned int y = 0; y < m_height; y++)
    {
      memcpy(dst, src, m_pitch);
      src += m_pitch;
      dst += pitch;
    }
  }
  return true;
}

bool CFFmpegImage::CreateThumbnailFromSurface(unsigned char* bufferin, unsigned int width,
                                             unsigned int height, unsigned int format,
                                             unsigned int pitch,
                                             const std::string& destFile,
                                             unsigned char* &bufferout,
                                             unsigned int &bufferoutSize)
{
  return false;
}

void CFFmpegImage::ReleaseThumbnailBuffer()
{
  av_freep(&m_pFrameRGB);
}
