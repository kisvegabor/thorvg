/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include <string.h>

#include "tvgInlist.h"
#include "tvgLoader.h"

#ifdef THORVG_SVG_LOADER_SUPPORT
    #include "tvgSvgLoader.h"
#endif

#ifdef THORVG_PNG_LOADER_SUPPORT
    #include "tvgPngLoader.h"
#endif

#ifdef THORVG_TVG_LOADER_SUPPORT
    #include "tvgTvgLoader.h"
#endif

#ifdef THORVG_JPG_LOADER_SUPPORT
    #include "tvgJpgLoader.h"
#endif

#ifdef THORVG_WEBP_LOADER_SUPPORT
    #include "tvgWebpLoader.h"
#endif

#ifdef THORVG_TTF_LOADER_SUPPORT
    #include "tvgTtfLoader.h"
#endif

#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    #include "tvgLottieLoader.h"
#endif

#include "tvgRawLoader.h"


uint64_t HASH_KEY(const char* data, uint64_t size)
{
    return (((uint64_t) data) << 32) | size;
}

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static mutex mtx;
static Inlist<LoadModule> _activeLoaders;


static LoadModule* _find(FileType type)
{
    switch(type) {
        case FileType::Tvg: {
#ifdef THORVG_TVG_LOADER_SUPPORT
            return new TvgLoader;
#endif
            break;
        }
        case FileType::Svg: {
#ifdef THORVG_SVG_LOADER_SUPPORT
            return new SvgLoader;
#endif
            break;
        }
        case FileType::Ttf: {
#ifdef THORVG_TTF_LOADER_SUPPORT
            return new TtfLoader;
#endif
            break;
        }
        case FileType::Lottie: {
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
            return new LottieLoader;
#endif
            break;
        }
        case FileType::Raw: {
            return new RawLoader;
            break;
        }
        case FileType::Png: {
#ifdef THORVG_PNG_LOADER_SUPPORT
            return new PngLoader;
#endif
            break;
        }
        case FileType::Jpg: {
#ifdef THORVG_JPG_LOADER_SUPPORT
            return new JpgLoader;
#endif
            break;
        }
        case FileType::Webp: {
#ifdef THORVG_WEBP_LOADER_SUPPORT
            return new WebpLoader;
#endif
            break;
        }
        default: {
            break;
        }
    }

#ifdef THORVG_LOG_ENABLED
    const char *format;
    switch(type) {
        case FileType::Tvg: {
            format = "TVG";
            break;
        }
        case FileType::Svg: {
            format = "SVG";
            break;
        }
        case FileType::Ttf: {
            format = "TTF";
            break;
        }
        case FileType::Lottie: {
            format = "lottie(json)";
            break;
        }
        case FileType::Raw: {
            format = "RAW";
            break;
        }
        case FileType::Png: {
            format = "PNG";
            break;
        }
        case FileType::Jpg: {
            format = "JPG";
            break;
        }
        case FileType::Webp: {
            format = "WEBP";
            break;
        }
        default: {
            format = "???";
            break;
        }
    }
    TVGLOG("RENDERER", "%s format is not supported", format);
#endif
    return nullptr;
}


static LoadModule* _findByPath(const string& path)
{
    auto ext = path.substr(path.find_last_of(".") + 1);
    if (!ext.compare("tvg")) return _find(FileType::Tvg);
    if (!ext.compare("svg")) return _find(FileType::Svg);
    if (!ext.compare("json")) return _find(FileType::Lottie);
    if (!ext.compare("lottie")) return _find(FileType::Lottie);
    if (!ext.compare("png")) return _find(FileType::Png);
    if (!ext.compare("jpg")) return _find(FileType::Jpg);
    if (!ext.compare("webp")) return _find(FileType::Webp);
    if (!ext.compare("ttf") || !ext.compare("ttc")) return _find(FileType::Ttf);
    if (!ext.compare("otf") || !ext.compare("otc")) return _find(FileType::Ttf);
    return nullptr;
}


static FileType _convert(const string& mimeType)
{
    auto type = FileType::Unknown;

    if (mimeType == "tvg") type = FileType::Tvg;
    else if (mimeType == "svg" || mimeType == "svg+xml") type = FileType::Svg;
    else if (mimeType == "ttf" || mimeType == "otf") type = FileType::Ttf;
    else if (mimeType == "lottie") type = FileType::Lottie;
    else if (mimeType == "raw") type = FileType::Raw;
    else if (mimeType == "png") type = FileType::Png;
    else if (mimeType == "jpg" || mimeType == "jpeg") type = FileType::Jpg;
    else if (mimeType == "webp") type = FileType::Webp;
    else TVGLOG("RENDERER", "Given mimetype is unknown = \"%s\".", mimeType.c_str());

    return type;
}


static LoadModule* _findByType(const string& mimeType)
{
    return _find(_convert(mimeType));
}


static LoadModule* _findFromCache(const string& path)
{
    unique_lock<mutex> lock{mtx};

    auto loader = _activeLoaders.head;

    while (loader) {
        if (loader->hashpath && !strcmp(loader->hashpath, path.c_str())) {
            ++loader->sharing;
            return loader;
        }
        loader = loader->next;
    }
    return nullptr;
}


static LoadModule* _findFromCache(const char* data, uint32_t size, const string& mimeType)
{
    auto type = _convert(mimeType);
    if (type == FileType::Unknown) return nullptr;

    unique_lock<mutex> lock{mtx};
    auto loader = _activeLoaders.head;

    auto key = HASH_KEY(data, size);

    while (loader) {
        if (loader->type == type && loader->hashkey == key) {
            ++loader->sharing;
            return loader;
        }
        loader = loader->next;
    }
    return nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool LoaderMgr::init()
{
    return true;
}


bool LoaderMgr::term()
{
    auto loader = _activeLoaders.head;

    //clean up the remained font loaders which is globally used.
    while (loader && loader->type == FileType::Ttf) {
        auto ret = loader->close();
        auto tmp = loader;
        loader = loader->next;
        _activeLoaders.remove(tmp);
        if (ret) delete(loader);
    }
    return true;
}


bool LoaderMgr::retrieve(LoadModule* loader)
{
    if (!loader) return false;
    if (loader->close()) {
        {
            unique_lock<mutex> lock{mtx};
            _activeLoaders.remove(loader);
        }
        delete(loader);
    }
    return true;
}


LoadModule* LoaderMgr::loader(const string& path, bool* invalid)
{
    *invalid = false;

    if (auto loader = _findFromCache(path)) return loader;

    if (auto loader = _findByPath(path)) {
        if (loader->open(path)) {
            loader->hashpath = strdup(path.c_str());
            {
                unique_lock<mutex> lock{mtx};
                _activeLoaders.back(loader);
            }
            return loader;
        }
        delete(loader);
        *invalid = true;
    }
    return nullptr;
}


bool LoaderMgr::retrieve(const string& path)
{
    return retrieve(_findFromCache(path));
}


LoadModule* LoaderMgr::loader(const char* key)
{
    auto loader = _activeLoaders.head;

    while (loader) {
        if (loader->hashpath && strstr(loader->hashpath, key)) {
            ++loader->sharing;
            return loader;
        }
        loader = loader->next;
    }
    return nullptr;
}


LoadModule* LoaderMgr::loader(const char* data, uint32_t size, const string& mimeType, const string& rpath, bool copy)
{
    if (auto loader = _findFromCache(data, size, mimeType)) return loader;

    //Try with the given MimeType
    if (!mimeType.empty()) {
        if (auto loader = _findByType(mimeType)) {
            if (loader->open(data, size, rpath, copy)) {
                loader->hashkey = HASH_KEY(data, size);                
                unique_lock<mutex> lock{mtx};
                _activeLoaders.back(loader);
                return loader;
            } else {
                TVGLOG("LOADER", "Given mimetype \"%s\" seems incorrect or not supported.", mimeType.c_str());
                delete(loader);
            }
        }
    //Unkown MimeType. Try with the candidates in the order
    } else {
        for (int i = 0; i < static_cast<int>(FileType::Unknown); i++) {
            auto loader = _find(static_cast<FileType>(i));
            if (loader) {
                if (loader->open(data, size, rpath, copy)) {
                    loader->hashkey = HASH_KEY(data, size);
                    {
                        unique_lock<mutex> lock{mtx};
                        _activeLoaders.back(loader);
                    }
                    return loader;
                }
                delete(loader);
            }
        }
    }
    return nullptr;
}


LoadModule* LoaderMgr::loader(const uint32_t *data, uint32_t w, uint32_t h, bool premultiplied, bool copy)
{
    //TODO: should we check premultiplied??
    if (auto loader = _findFromCache((const char*)(data), w * h, "raw")) return loader;

    //function is dedicated for raw images only
    auto loader = new RawLoader;
    if (loader->open(data, w, h, premultiplied, copy)) {
        loader->hashkey = HASH_KEY((const char*)data, w * h);
        {
            unique_lock<mutex> lock{mtx};
            _activeLoaders.back(loader);
        }
        return loader;
    }
    delete(loader);
    return nullptr;
}