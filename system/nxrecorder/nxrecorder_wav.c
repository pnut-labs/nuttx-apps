/****************************************************************************
 * apps/system/nxrecorder/nxrecorder_wav.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>

#include "system/nxrecorder.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* A canonical WAV file's header: RIFF, its fmt chunk for PCM, and the data
 * chunk's own header
 */

#define WAV_HEADER_SIZE   44
#define WAV_RIFF_SIZE_AT  4
#define WAV_DATA_SIZE_AT  40

/* A size too large for the header: readers take it as "to the end" */

#define WAV_SIZE_UNKNOWN  0xffffffff

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxrecorder_wav_put16, nxrecorder_wav_put32
 *
 *   Little-endian, as WAV wants whatever the processor's order.
 *
 ****************************************************************************/

static void nxrecorder_wav_put16(FAR uint8_t *p, uint16_t value)
{
  p[0] = value & 0xff;
  p[1] = value >> 8;
}

static void nxrecorder_wav_put32(FAR uint8_t *p, uint32_t value)
{
  p[0] = value & 0xff;
  p[1] = (value >> 8) & 0xff;
  p[2] = (value >> 16) & 0xff;
  p[3] = value >> 24;
}

/****************************************************************************
 * Name: nxrecorder_wav_write
 *
 *   Write all of a buffer, at the file's position.
 *
 ****************************************************************************/

static int nxrecorder_wav_write(int fd, FAR const uint8_t *buf, size_t len)
{
  ssize_t n;

  while (len > 0)
    {
      n = write(fd, buf, len);
      if (n < 0 && errno == EINTR)
        {
          continue;
        }

      if (n <= 0)
        {
          return n < 0 ? -errno : -EIO;
        }

      buf += n;
      len -= n;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxrecorder_write_wav
 *
 *   nxrecorder_write_wav() writes a WAV file's header, its sizes left at
 *   zero until nxrecorder_finish_wav() fills them in.
 *
 ****************************************************************************/

int nxrecorder_write_wav(int fd, uint32_t samplerate,
                         uint8_t chans, uint8_t bps)
{
  uint8_t header[WAV_HEADER_SIZE] =
  {
    'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
    'f', 'm', 't', ' '
  };

  uint32_t block = (uint32_t)chans * ((bps + 7) / 8);

  if (chans == 0 || bps == 0 || samplerate == 0)
    {
      return -EINVAL;
    }

  nxrecorder_wav_put32(header + 16, 16);          /* fmt chunk's size */
  nxrecorder_wav_put16(header + 20, 1);           /* PCM */
  nxrecorder_wav_put16(header + 22, chans);
  nxrecorder_wav_put32(header + 24, samplerate);
  nxrecorder_wav_put32(header + 28, samplerate * block);
  nxrecorder_wav_put16(header + 32, block);
  nxrecorder_wav_put16(header + 34, bps);
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';

  return nxrecorder_wav_write(fd, header, sizeof(header));
}

/****************************************************************************
 * Name: nxrecorder_finish_wav
 *
 *   nxrecorder_finish_wav() fills in a WAV file's sizes once the recording
 *   has ended, from the file's own size.
 *
 ****************************************************************************/

int nxrecorder_finish_wav(int fd)
{
  uint8_t size[4];
  off_t end;
  int ret;

  end = lseek(fd, 0, SEEK_END);
  if (end < WAV_HEADER_SIZE)
    {
      return end < 0 ? -errno : -EINVAL;
    }

  nxrecorder_wav_put32(size, (uint64_t)end - 8 > WAV_SIZE_UNKNOWN ?
                             WAV_SIZE_UNKNOWN : (uint32_t)(end - 8));
  if (lseek(fd, WAV_RIFF_SIZE_AT, SEEK_SET) < 0)
    {
      return -errno;
    }

  ret = nxrecorder_wav_write(fd, size, sizeof(size));
  if (ret < 0)
    {
      return ret;
    }

  nxrecorder_wav_put32(size,
                       (uint64_t)end - WAV_HEADER_SIZE > WAV_SIZE_UNKNOWN ?
                       WAV_SIZE_UNKNOWN : (uint32_t)(end - WAV_HEADER_SIZE));
  if (lseek(fd, WAV_DATA_SIZE_AT, SEEK_SET) < 0)
    {
      return -errno;
    }

  return nxrecorder_wav_write(fd, size, sizeof(size));
}
