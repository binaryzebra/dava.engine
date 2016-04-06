/*==================================================================================
	Copyright (c) 2008, binaryzebra
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.
	* Neither the name of the binaryzebra nor the
	names of its contributors may be used to endorse or promote products
	derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	=====================================================================================*/


#include "TexturePacker/DefinitionFile.h"
#include "TexturePacker/PngImage.h"
#include "Render/Image/LibPSDHelper.h"
#include <libpng/png.h>
#include <libpsd/libpsd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "FileSystem/FileSystem.h"
#include "FramePathHelper.h"

namespace DAVA
{
namespace DefinitionFileLocal
{
bool WritePNGImage(int width, int height, char* imageData, const char* outName, int channels, int bit_depth);
}

void DefinitionFile::LoadPNG(const FilePath& _filename, const FilePath& pathToProcess)
{
    DVASSERT(pathToProcess.IsDirectoryPathname());

    String nameWithoutExt = _filename.GetBasename();
    FilePath corespondingPngImage = FilePath::CreateWithNewExtension(_filename, ".png");

    filename = pathToProcess + (nameWithoutExt + String(".txt"));
    frameCount = 1;

    PngImageExt image;
    image.Read(corespondingPngImage);
    spriteWidth = image.GetWidth();
    spriteHeight = image.GetHeight();

    frameNames.resize(frameCount);
    frameRects.resize(frameCount);
    frameRects[0].x = 0;
    frameRects[0].y = 0;
    frameRects[0].dx = spriteWidth;
    frameRects[0].dy = spriteHeight;

    FilePath fileWrite = FramePathHelper::GetFramePathAbsolute(pathToProcess, nameWithoutExt, 0);
    FileSystem::Instance()->CopyFile(_filename, fileWrite);
}

bool DefinitionFile::LoadPNGDef(const FilePath& _filename, const FilePath& pathToProcess)
{
    DVASSERT(pathToProcess.IsDirectoryPathname());

    Logger::FrameworkDebug("* Load PNG Definition: %s", _filename.GetAbsolutePathname().c_str());

    FILE* fp = fopen(_filename.GetAbsolutePathname().c_str(), "rt");
    fscanf(fp, "%d", &frameCount);

    String nameWithoutExt = _filename.GetBasename();
    FilePath corespondingPngImage = _filename.GetDirectory() + (nameWithoutExt + String(".png"));

    filename = pathToProcess + (nameWithoutExt + String(".txt"));

    PngImageExt image;
    image.Read(corespondingPngImage);
    spriteWidth = image.GetWidth() / frameCount;
    spriteHeight = image.GetHeight();

    Logger::FrameworkDebug("* frameCount: %d spriteWidth: %d spriteHeight: %d", frameCount, spriteWidth, spriteHeight);

    frameRects.resize(frameCount);
    frameNames.resize(frameCount);
    for (uint32 k = 0; k < frameCount; ++k)
    {
        PngImageExt frameX;
        frameX.Create(spriteWidth, spriteHeight);
        frameX.DrawImage(0, 0, &image, Rect2i(k * spriteWidth, 0, spriteWidth, spriteHeight));

        Rect2i reducedRect;
        frameX.FindNonOpaqueRect(reducedRect);
        Logger::FrameworkDebug("%s - reduced_rect(%d %d %d %d)", nameWithoutExt.c_str(), reducedRect.x, reducedRect.y, reducedRect.dx, reducedRect.dy);

        PngImageExt frameX2;
        frameX2.Create(reducedRect.dx, reducedRect.dy);
        frameX2.DrawImage(0, 0, &frameX, reducedRect);

        FilePath fileWrite = FramePathHelper::GetFramePathAbsolute(pathToProcess, nameWithoutExt, k);
        frameX2.Write(fileWrite);

        frameRects[k].x = reducedRect.x;
        frameRects[k].y = reducedRect.y;
        frameRects[k].dx = reducedRect.dx;
        frameRects[k].dy = reducedRect.dy;
    }

    fclose(fp);
    return true;
}

bool DefinitionFile::Load(const FilePath& _filename)
{
    filename = _filename;
    FILE* fp = fopen(filename.GetAbsolutePathname().c_str(), "rt");
    if (!fp)
    {
        Logger::Error("*** ERROR: Can't open definition file: %s", filename.GetAbsolutePathname().c_str());
        return false;
    }
    fscanf(fp, "%d %d", &spriteWidth, &spriteHeight);
    fscanf(fp, "%d", &frameCount);

    frameRects.resize(frameCount);
    frameNames.resize(frameCount);
    for (uint32 i = 0; i < frameCount; ++i)
    {
        char frameName[128];
        fscanf(fp, "%d %d %d %d %s\n", &frameRects[i].x, &frameRects[i].y, &frameRects[i].dx, &frameRects[i].dy, frameName);
        Logger::FrameworkDebug("[DefinitionFile] frame: %d w: %d h: %d", i, frameRects[i].dx, frameRects[i].dy);
        frameNames[i] = String(frameName);
    }

    while (1)
    {
        char tmpString[512];
        fgets(tmpString, sizeof(tmpString), fp);
        pathsInfo.push_back(tmpString);
        printf("str: %s\n", tmpString);
        if (feof(fp))
            break;
    }

    fclose(fp);
    Logger::FrameworkDebug("Loaded definition: %s frames: %d", filename.GetAbsolutePathname().c_str(), frameCount);

    return true;
}

Size2i DefinitionFile::GetFrameSize(uint32 frame) const
{
    return Size2i(frameRects[frame].dx, frameRects[frame].dy);
}

int DefinitionFile::GetFrameWidth(uint32 frame) const
{
    return frameRects[frame].dx;
}

int DefinitionFile::GetFrameHeight(uint32 frame) const
{
    return frameRects[frame].dy;
}

bool DefinitionFile::LoadPSD(const FilePath& fullname, const FilePath& processDirectoryPath, uint32 maxTextureSize, bool withAlpha, bool useLayerNames)
{
    if (FileSystem::Instance()->CreateDirectory(processDirectoryPath) == FileSystem::DIRECTORY_CANT_CREATE)
    {
        Logger::Error("============================ ERROR ============================");
        Logger::Error("| Can't create output directory: ");
        Logger::Error("| %s", processDirectoryPath.GetAbsolutePathname().c_str());
        Logger::Error("===============================================================");
        return false;
    }

    auto psdNameString = fullname.GetAbsolutePathname();
    const char* psdName = psdNameString.c_str();

    psd_context* psd = nullptr;
    auto status = psd_image_load(&psd, const_cast<psd_char*>(psdName));
    if ((psd == nullptr) || (status != psd_status_done))
    {
        Logger::Error("============================ ERROR ============================");
        Logger::Error("| Unable to load PSD from file (result code = %d):", static_cast<int>(status));
        Logger::Error("| %s", psdName);
        Logger::Error("| Try to re-save this file by using `Save as...` in Photoshop");
        Logger::Error("===============================================================");
        return false;
    }

    SCOPE_EXIT
    {
        psd_image_free(psd);
    };

    auto outImageBasePath = fullname;
    outImageBasePath.ReplaceExtension(".png");

    auto outImageBaseName = outImageBasePath.GetBasename();
    filename = processDirectoryPath + outImageBaseName + ".txt";

    frameCount = psd->layer_count;
    frameRects.resize(frameCount);
    frameNames.resize(frameCount);
    pathsInfo.resize(frameCount);
    spriteWidth = psd->width;
    spriteHeight = psd->height;

    outImageBasePath.ReplaceDirectory(processDirectoryPath);
    for (uint32 lIndex = 0; lIndex < frameCount; ++lIndex)
    {
        auto& layer = psd->layer_records[lIndex];
        auto layerName = String(reinterpret_cast<const char*>(layer.layer_name));

        if (layer.width * layer.height == 0)
        {
            Logger::Error("============================ ERROR ============================");
            Logger::Error("| File contains empty layer %d (%s)", lIndex, layerName.c_str());
            Logger::Error("| %s", psdName);
            Logger::Error("===============================================================");
            return false;
        }

        outImageBasePath.ReplaceBasename(outImageBaseName + "_" + std::to_string(lIndex));

        if (layerName.empty())
        {
            // Logger::Warning("=========================== WARNING ===========================");
            // Logger::Warning("| PSD file: %s", psdName);
            // Logger::Warning("| Contains layer without a name: %u", static_cast<int32>(lIndex));
            // Logger::Warning("===============================================================");
        }
        else if (std::find(frameNames.begin(), frameNames.end(), layerName) != frameNames.end())
        {
            // Logger::Warning("=========================== WARNING ===========================");
            // Logger::Warning("| PSD file:");
            // Logger::Warning("| %s", psdName);
            // Logger::Warning("| Contains two or more layers with the same name: %s", layerName.c_str());
            // Logger::Warning("===============================================================");
        }

        if (layer.opacity < 255)
        {
            Logger::Warning("============================ Warning ============================");
            Logger::Warning("| File contains layer `%s` with opacity less than 100%% (%.0f)", layerName.c_str(), 100.0f * static_cast<float>(layer.opacity) / 255.0f);
            Logger::Warning("| %s", psdName);
            Logger::Warning("=================================================================");
        }

        frameNames[lIndex] = layerName;
        pathsInfo[lIndex] = useLayerNames || layerName.empty() ? layerName : String("frame") + std::to_string(lIndex);

        uint32* sourceData = reinterpret_cast<uint32*>(layer.image_data);
        for (psd_int i = 0, e = layer.width * layer.height; i < e; ++i)
        {
            sourceData[i] = (sourceData[i] & 0xff00ff00) | (sourceData[i] & 0x000000ff) << 16 | (sourceData[i] & 0xff0000) >> 16;
        }

        bool shouldManuallyReleaseData = false;
        auto imageData = reinterpret_cast<char*>(layer.image_data);
        int imageWidth = layer.width;
        int imageHeight = layer.height;

        if ((layer.left < 0) || (layer.right > 0) || (layer.width > psd->width) || (layer.height > psd->height))
        {
            int xOffset = Max(0, -layer.left);
            int yOffset = Max(0, -layer.top);
            imageWidth = Min(layer.width - xOffset, psd->width);
            imageHeight = Min(layer.height - yOffset, psd->height);

            Logger::Warning("============================ Warning ============================");
            Logger::Warning("| File contains layer `%s` which is larger than canvas", layerName.c_str());
            Logger::Warning("| %s", psdName);
            Logger::Warning("=================================================================");

            size_type rowSize = 4 * imageWidth;

            uint32* croppedData = reinterpret_cast<uint32*>(calloc(imageWidth * imageHeight, 4));
            for (int y = 0; y < imageHeight; ++y)
            {
                auto src = sourceData + xOffset + (yOffset + y) * layer.width;
                auto dst = croppedData + y * imageWidth;
                memcpy(dst, src, rowSize);
                DVASSERT(memcmp(src, dst, rowSize) == 0);
            }
            imageData = reinterpret_cast<char*>(croppedData);
            shouldManuallyReleaseData = true;
        }

        if (DefinitionFileLocal::WritePNGImage(imageWidth, imageHeight, imageData, outImageBasePath.GetAbsolutePathname().c_str(), 4, 8) == false)
        {
            Logger::Error("============================ ERROR ============================");
            Logger::Error("| Failed to write PNG to file:");
            Logger::Error("| %s", outImageBasePath.GetAbsolutePathname().c_str());
            Logger::Error("| Input file (layer %u):", static_cast<int32>(lIndex));
            Logger::Error("| %s", psdName);
            Logger::Error("===============================================================");
            return false;
        }

        if (shouldManuallyReleaseData)
        {
            free(imageData);
        }

        frameRects[lIndex] = Rect2i(layer.left, layer.top, layer.width, layer.height);

        if (withAlpha)
        {
            int32 iMaxTextureSize = static_cast<int32>(maxTextureSize);
            if ((frameRects[lIndex].dx > iMaxTextureSize) || (frameRects[lIndex].dy > iMaxTextureSize))
            {
                Logger::Warning("Frame of %s layer %d is bigger than maxTextureSize (%d) layer exportSize (%d x %d) FORCE REDUCE TO (%d x %d)",
                                psdName, lIndex, maxTextureSize, frameRects[lIndex].dx, frameRects[lIndex].dy, spriteWidth, spriteHeight);
                Logger::Warning("Bewarned!!! Results not guaranteed!!!");
                frameRects[lIndex].dx = spriteWidth;
                frameRects[lIndex].dy = spriteHeight;
            }
            else
            {
                if (frameRects[lIndex].dx > spriteWidth)
                {
                    Logger::Warning("For texture %s, layer %d width is bigger than sprite width: %d > %d", psdName, lIndex, frameRects[lIndex].dx, spriteWidth);
                    Logger::Warning("Layer width will be reduced to the sprite value");
                    frameRects[lIndex].dx = spriteWidth;
                }

                if (frameRects[lIndex].dy > spriteHeight)
                {
                    Logger::Warning("For texture %s, layer %d height is bigger than sprite height: %d > %d", psdName, lIndex, frameRects[lIndex].dy, spriteHeight);
                    Logger::Warning("Layer height will be reduced to the sprite value");
                    frameRects[lIndex].dy = spriteHeight;
                }
            }
        }
    }

    return true;
}

namespace DefinitionFileLocal
{
bool WritePNGImage(int width, int height, char* imageData, const char* outName, int channels, int bit_depth)
{
    FILE* fp = fopen(outName, "wb");
    if (fp == nullptr)
        return false;

    SCOPE_EXIT
    {
        fclose(fp);
    };

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == nullptr)
        return false;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr)
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_init_io(png_ptr, fp);

    png_byte colorType = 0;
    switch (channels)
    {
    case 1:
    {
        colorType = PNG_COLOR_TYPE_GRAY;
        break;
    }
    case 2:
    {
        colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
        break;
    }

    case 3:
    {
        colorType = PNG_COLOR_TYPE_RGB;
        break;
    }
    case 4:
    {
        colorType = PNG_COLOR_TYPE_RGBA;
        break;
    }

    default:
    {
        printf("Invalid image configuration: %d channels, %d bit depth", channels, bit_depth);
        png_destroy_info_struct(png_ptr, &info_ptr);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }
    }

    int rowSize = width * channels * bit_depth / 8;

    png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, colorType,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    for (int y = 0; y < height; ++y)
        png_write_row(png_ptr, reinterpret_cast<png_bytep>(&imageData[y * rowSize]));
    png_write_end(png_ptr, info_ptr);
    png_destroy_info_struct(png_ptr, &info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return true;
}
}
}