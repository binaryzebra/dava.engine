#ifndef __DAVAENGINE_TGA_HELPER_H__
#define __DAVAENGINE_TGA_HELPER_H__

#include <memory>

#include "Render/Image/Image.h"
#include "Render/Image/ImageFormatInterface.h"

namespace DAVA
{
class LibTgaHelper : public ImageFormatInterface
{
public:
    LibTgaHelper();

    bool CanProcessFile(const ScopedPtr<File>& infile) const override;

    eErrorCode ReadFile(const ScopedPtr<File>& infile, Vector<Image*>& imageSet, const ImageSystem::LoadingParams& loadingParams) const override;

    eErrorCode WriteFile(const FilePath& fileName, const Vector<Image*>& imageSet, PixelFormat compressionFormat, ImageQuality quality) const override;
    eErrorCode WriteFileAsCubeMap(const FilePath& fileName, const Vector<Vector<Image*>>& imageSet, PixelFormat compressionFormat, ImageQuality quality) const override;

    ImageInfo GetImageInfo(const ScopedPtr<File>& infile) const override;

    struct TgaInfo
    {
        enum IMAGE_TYPE : uint8
        {
            TRUECOLOR = 2,
            GRAYSCALE = 3,
            COMPRESSED_TRUECOLOR = 10,
            COMPRESSED_GRAYSCALE = 11
        };
        enum ORIGIN_CORNER : uint8
        {
            BOTTOM_LEFT = 0,
            BOTTOM_RIGHT = 1,
            TOP_LEFT = 2,
            TOP_RIGHT = 3
        };

        uint16 width;
        uint16 height;
        uint8 bytesPerPixel;
        uint8 alphaBits;
        IMAGE_TYPE imageType;
        ORIGIN_CORNER origin_corner;
        PixelFormat pixelFormat;
    };

    eErrorCode ReadTgaHeader(const FilePath& filepath, TgaInfo& tgaHeader) const;

private:
    eErrorCode ReadTgaHeader(File* infile, TgaInfo& tgaHeader) const;

    eErrorCode ReadCompressedTga(File* infile, const TgaInfo& tgaHeader, ScopedPtr<Image>& image) const;
    eErrorCode ReadUncompressedTga(File* infile, const TgaInfo& tgaHeader, ScopedPtr<Image>& image) const;
    PixelFormat DefinePixelFormat(const TgaInfo& tgaHeader) const;

    eErrorCode WriteTgaHeader(File* outfile, const TgaInfo& tgaHeader) const;
    eErrorCode WriteUncompressedTga(File* infile, const TgaInfo& tgaHeader, const uint8* data) const;

    struct ImageDataWriter
    {
        ImageDataWriter(Image* image, const LibTgaHelper::TgaInfo& tgaInfo);
        inline bool AtEnd() const
        {
            return isAtEnd;
        }
        void Write(uint8* pixel);

    private:
        void ResetPtr(const Image* image, const LibTgaHelper::TgaInfo& tgaInfo);
        void IncrementPtr();

    public:
        uint8* ptr;
        ptrdiff_t ptrNextLineJump;
        ptrdiff_t ptrInc;

    private:
        const TgaInfo& tgaInfo;
        uint16 linesRemaining;
        uint16 linePixelsRemaining;
        bool isAtEnd;
    };
};
};

#endif // __DAVAENGINE_TGA_HELPER_H__