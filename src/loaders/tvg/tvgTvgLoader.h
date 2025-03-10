/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_TVG_LOADER_H_
#define _TVG_TVG_LOADER_H_

#include "tvgTaskScheduler.h"
#include "tvgTvgCommon.h"


class TvgLoader : public ImageLoader, public Task
{
public:
    const char* data = nullptr;
    const char* ptr = nullptr;
    uint32_t size = 0;
    uint16_t version = 0;
    Scene* root = nullptr;
    TvgBinInterpreterBase* interpreter = nullptr;
    uint32_t uncompressedSize = 0;
    uint32_t compressedSize = 0;
    uint32_t compressedSizeBits = 0;
    bool copy = false;
    bool compressed = false;

    TvgLoader();
    ~TvgLoader();

    bool open(const string &path) override;
    bool open(const char *data, uint32_t size, const string& rpath, bool copy) override;
    bool read() override;
    bool resize(Paint* paint, float w, float h) override;

    Paint* paint() override;

private:
    bool readHeader();
    void run(unsigned tid) override;
    void clear();
};

#endif //_TVG_TVG_LOADER_H_
