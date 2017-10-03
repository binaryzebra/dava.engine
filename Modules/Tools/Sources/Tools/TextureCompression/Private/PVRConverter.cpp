#include "Tools/TextureCompression/PVRConverter.h"

#include "Render/TextureDescriptor.h"
#include "FileSystem/FileSystem.h"
#include "Utils/StringFormat.h"
#include "Platform/Process.h"
#include "Logger/Logger.h"

#include "Render/GPUFamilyDescriptor.h"
#include "Render/Image/LibPVRHelper.h"
#include "Render/Image/LibTgaHelper.h"
#include "Render/Image/ImageSystem.h"
#include "Render/Image/ImageConvert.h"
#include "Render/Image/Image.h"

#include "Base/GlobalEnum.h"

namespace DAVA
{
static String CUBEMAP_TMP_DIR = "~doc:/ResourceEditor_Cubemap_Tmp/";

static Array<String, Texture::CUBE_FACE_COUNT> PVRTOOL_FACE_SUFFIXES =
{ {
String("1"), //pz
String("2"), //nz
String("3"), //px
String("4"), //nx
String("5"), //pz
String("6") //nz
} };

static DAVA::String PVR_QUALITY_SETTING[] =
{
  "pvrtcfastest",
  "pvrtcfast",
  "pvrtcnormal",
  "pvrtchigh",
  "pvrtcbest"
};

static DAVA::String ETC_QUALITY_SETTING[] =
{
  "etcfast",
  "etcfast",
  "etcslow",
  "etcfastperceptual",
  "etcslowperceptual"
};

PVRConverter::PVRConverter()
{
    // pvr map
    pixelFormatToPVRFormat[FORMAT_RGBA8888] = "r8g8b8a8"; //"OGL8888";
    pixelFormatToPVRFormat[FORMAT_RGBA4444] = "r4g4b4a4"; //"OGL4444";
    pixelFormatToPVRFormat[FORMAT_RGBA5551] = "r5g5b5a1"; //"OGL5551";
    pixelFormatToPVRFormat[FORMAT_RGB565] = "r5g6b5"; //"OGL565";
    pixelFormatToPVRFormat[FORMAT_RGB888] = "r8g8b8"; //"OGL888";
    pixelFormatToPVRFormat[FORMAT_PVR2] = "PVRTC1_2";
    pixelFormatToPVRFormat[FORMAT_PVR4] = "PVRTC1_4";
    pixelFormatToPVRFormat[FORMAT_A8] = "l8"; //"OGL8";
    pixelFormatToPVRFormat[FORMAT_ETC1] = "ETC1";

    pixelFormatToPVRFormat[FORMAT_PVR2_2] = "PVRTC2_2";
    pixelFormatToPVRFormat[FORMAT_PVR4_2] = "PVRTC2_4";
    pixelFormatToPVRFormat[FORMAT_EAC_R11_UNSIGNED] = "EAC_R11";
    pixelFormatToPVRFormat[FORMAT_EAC_R11_SIGNED] = "EAC_R11";
    pixelFormatToPVRFormat[FORMAT_EAC_RG11_UNSIGNED] = "EAC_RG11";
    pixelFormatToPVRFormat[FORMAT_EAC_RG11_SIGNED] = "EAC_RG11";
    pixelFormatToPVRFormat[FORMAT_ETC2_RGB] = "ETC2_RGB";
    pixelFormatToPVRFormat[FORMAT_ETC2_RGBA] = "ETC2_RGBA";
    pixelFormatToPVRFormat[FORMAT_ETC2_RGB_A1] = "ETC2_RGB_A1";

    pixelFormatToPVRFormat[FORMAT_RGBA16F] = "r16g16b16a16,SF";
    pixelFormatToPVRFormat[FORMAT_RGBA32F] = "r32g32b32a32,SF";
}

PVRConverter::~PVRConverter()
{
}

Vector<Image*> ProcessFloatImageSet(Vector<Image*>& sourceImages, PixelFormat inputFormat, const TextureDescriptor::Compression* compression, bool generateMipmaps)
{
    Vector<Image*> outputImages;

    if ((compression->compressToHeight != 0) && (compression->compressToWidth != 0))
    {
        for (Image* sourceImage : sourceImages)
        {
            if (!sourceImage->ResizeImage(compression->compressToWidth, compression->compressToHeight))
            {
                Logger::Error("Failed to resize source images");
                return outputImages;
            }
        }
    }

    if (generateMipmaps)
    {
        Vector<Image*> generatedImages;
        for (Image* sourceImage : sourceImages)
        {
            if (sourceImage->mipmapLevel == 0)
            {
                Vector<Image*> mipMaps = sourceImage->CreateMipMapsImages(false);
                generatedImages.insert(generatedImages.end(), mipMaps.begin(), mipMaps.end());
            }
        }

        // release original images and replace them with generated ones
        for (Image* image : sourceImages)
            SafeRelease(image);
        sourceImages.swap(generatedImages);
    }
    outputImages.reserve(sourceImages.size());

    for (Image* sourceImage : sourceImages)
    {
        const PixelFormat targetFormat = static_cast<PixelFormat>(compression->format);
        Image* targetImage = Image::Create(sourceImage->width, sourceImage->height, targetFormat);
        if (ImageConvert::ConvertFloatFormats(sourceImage->width, sourceImage->height, inputFormat, targetFormat, sourceImage->data, targetImage->data))
        {
            outputImages.emplace_back(targetImage);
        }
        else
        {
            SafeRelease(targetImage);
        }
    }

    return outputImages;
}

FilePath PVRConverter::ConvertFloatTexture(const TextureDescriptor& descriptor, eGPUFamily gpuFamily, TextureConverter::eConvertQuality quality, const ImageInfo& sourceInfo)
{
    Vector<Image*> sourceImages;
    ImageSystem::Load(descriptor.GetSourceTexturePathname(), sourceImages);
    if (sourceImages.empty())
    {
        Logger::Error("Failed to convert float texture to PVR file. Failed to load source image");
        return FilePath();
    }

    const TextureDescriptor::Compression* compression = &descriptor.compression[gpuFamily];
    Vector<Image*> targetImages = ProcessFloatImageSet(sourceImages, sourceInfo.format, compression, descriptor.GetGenerateMipMaps());

    for (Image* image : sourceImages)
        SafeRelease(image);

    FilePath outputName = GetConvertedTexturePath(descriptor, gpuFamily);
    eErrorCode saveResult = LibPVRHelper().Save(outputName, targetImages);
    if (saveResult != eErrorCode::SUCCESS)
    {
        Logger::Error("Failed to save converted float texture to PVR file. Error code: %u", static_cast<uint32>(saveResult));
        outputName = FilePath();
    }

    for (Image* image : targetImages)
        SafeRelease(image);

    return outputName;
}

FilePath PVRConverter::ConvertFloatCubeTexture(const TextureDescriptor& descriptor, eGPUFamily gpuFamily, TextureConverter::eConvertQuality quality, const ImageInfo& sourceInfo)
{
    Vector<FilePath> faces;
    descriptor.GenerateFacePathnames(CUBEMAP_TMP_DIR, PVRTOOL_FACE_SUFFIXES, faces);
    Vector<Vector<Image*>> imagesToSave;

    for (const FilePath& face : faces)
    {
        Vector<Image*> sourceImages;
        ImageSystem::Load(face, sourceImages);
        if (sourceImages.empty())
        {
            Logger::Error("Failed to convert float texture to PVR file. Failed to load source image");
            return FilePath();
        }

        const TextureDescriptor::Compression* compression = &descriptor.compression[gpuFamily];
        imagesToSave.emplace_back(ProcessFloatImageSet(sourceImages, sourceInfo.format, compression, descriptor.GetGenerateMipMaps()));
        for (Image* image : sourceImages)
            SafeRelease(image);
    }

    FilePath outputName = GetConvertedTexturePath(descriptor, gpuFamily);
    eErrorCode saveResult = LibPVRHelper().SaveCubeMap(outputName, imagesToSave);
    if (saveResult != eErrorCode::SUCCESS)
    {
        Logger::Error("Failed to save converted float texture to PVR file. Error code: %u", static_cast<uint32>(saveResult));
        outputName = FilePath();
    }

    for (Vector<Image*>& face : imagesToSave)
    {
        for (Image* mip : face)
            SafeRelease(mip);
    }

    return outputName;
}

FilePath PVRConverter::ConvertToPvr(const TextureDescriptor& descriptor, eGPUFamily gpuFamily, TextureConverter::eConvertQuality quality, bool addCRC /* = true */)
{
#ifdef __DAVAENGINE_WIN_UAP__

    __DAVAENGINE_WIN_UAP_INCOMPLETE_IMPLEMENTATION__
    return FilePath();

#else
    FilePath inputPathname = descriptor.GetSourceTexturePathname();
    if (descriptor.IsCubeMap())
    {
        inputPathname = PrepareCubeMapForPvrConvert(descriptor);
    }

    FilePath outputName;
    ImageInfo sourceInfo = ImageSystem::GetImageInfo(inputPathname);
    if (PixelFormatDescriptor::IsFloatPixelFormat(sourceInfo.format))
    {
        if (descriptor.IsCubeMap())
        {
            outputName = ConvertFloatCubeTexture(descriptor, gpuFamily, quality, sourceInfo);
        }
        else
        {
            outputName = ConvertFloatTexture(descriptor, gpuFamily, quality, sourceInfo);
        }
    }
    else
    {
        Vector<String> args;
        GetToolCommandLine(descriptor, inputPathname, gpuFamily, quality, args);
        Process process(pvrTexToolPathname, args);
        if (process.Run(false))
        {
            process.Wait();

            const String& procOutput = process.GetOutput();
            if (procOutput.size() > 0)
            {
                Logger::FrameworkDebug(procOutput.c_str());
            }

            if (process.GetExitCode() != 0)
            {
                Logger::Error("PvrTexTool exited with code %d", process.GetExitCode());
                return FilePath();
            }

            outputName = GetConvertedTexturePath(descriptor, gpuFamily);
        }
        else
        {
            Logger::Error("Failed to run PVR tool! %s", pvrTexToolPathname.GetAbsolutePathname().c_str());
            return FilePath();
        }
    }

    if (descriptor.IsCubeMap())
    {
        CleanupCubemapAfterConversion(descriptor);
    }

    if (addCRC)
    {
        LibPVRHelper::AddCRCIntoMetaData(outputName);
    }

    return outputName;
#endif
}

FilePath PVRConverter::ConvertNormalMapToPvr(const TextureDescriptor& descriptor, eGPUFamily gpuFamily, TextureConverter::eConvertQuality quality)
{
    FilePath sourcePath = descriptor.GetSourceTexturePathname();

    ScopedPtr<Image> image(ImageSystem::LoadSingleMip(sourcePath));

    if (!image)
    {
        return FilePath();
    }

    bool normalized = image->Normalize();
    if (!normalized)
    {
        Logger::Error("[PVRConverter::ConvertNormalMapToPvr] Cannot normalize image %s", sourcePath.GetStringValue().c_str());
        return FilePath();
    }

    Vector<Image*> srcImages;
    Vector<Image*> convertedImages;
    SCOPE_EXIT
    {
        for_each(srcImages.begin(), srcImages.end(), SafeRelease<Image>);
        for_each(convertedImages.begin(), convertedImages.end(), SafeRelease<Image>);
    };
    if (descriptor.GetGenerateMipMaps())
    {
        srcImages = image->CreateMipMapsImages(true);
    }
    else
    {
        image->Retain();
        srcImages.push_back(image);
    }

    ImageFormat targetFormat = IMAGE_FORMAT_PVR;

    TextureDescriptor tempFileDescriptor;
    tempFileDescriptor.Initialize(&descriptor);
    tempFileDescriptor.SetGenerateMipmaps(false);
    tempFileDescriptor.dataSettings.sourceFileFormat = targetFormat;
    tempFileDescriptor.dataSettings.sourceFileExtension = ImageSystem::GetDefaultExtension(targetFormat);
    tempFileDescriptor.pathname.ReplaceBasename(sourcePath.GetBasename() + "_temp");
    tempFileDescriptor.compression[eGPUFamily::GPU_ORIGIN].format = image->format;
    tempFileDescriptor.compression[eGPUFamily::GPU_ORIGIN].imageFormat = targetFormat;

    FilePath tempSourcePath = tempFileDescriptor.GetSourceTexturePathname();
    FilePath tempConvertedPath = GetConvertedTexturePath(tempFileDescriptor, gpuFamily);
    SCOPE_EXIT
    {
        FileSystem::Instance()->DeleteFile(tempFileDescriptor.pathname);
        FileSystem::Instance()->DeleteFile(tempSourcePath);
        FileSystem::Instance()->DeleteFile(tempConvertedPath);
    };

    uint32 requestedWidth = tempFileDescriptor.compression[gpuFamily].compressToWidth;
    uint32 requestedHeight = tempFileDescriptor.compression[gpuFamily].compressToHeight;

    bool needSkipImages = (requestedWidth != 0 && requestedHeight != 0);
    for (Image* srcImage : srcImages)
    {
        if (needSkipImages && (srcImage->width > requestedWidth || srcImage->height > requestedHeight))
        {
            continue;
        }

        if (ImageSystem::Save(tempSourcePath, srcImage, image->format) != eErrorCode::SUCCESS)
        {
            return FilePath();
        }

        FilePath convertedImgPath = ConvertToPvr(tempFileDescriptor, gpuFamily, quality, false);
        if (convertedImgPath.IsEmpty())
        {
            return FilePath();
        }
        DVASSERT(convertedImgPath == tempConvertedPath);

        Image* convertedImage = ImageSystem::LoadSingleMip(convertedImgPath);
        if (!convertedImage)
        {
            return FilePath();
        }

        convertedImages.emplace_back(convertedImage);
    }

    DVASSERT(convertedImages.empty() == false);

    FilePath convertedTexturePath = GetConvertedTexturePath(descriptor, gpuFamily);
    if (convertedImages.empty() || (ImageSystem::Save(convertedTexturePath, convertedImages, convertedImages.front()->format) != eErrorCode::SUCCESS))
    {
        return FilePath();
    }

    LibPVRHelper::AddCRCIntoMetaData(convertedTexturePath);
    return convertedTexturePath;
}

void PVRConverter::GetToolCommandLine(const TextureDescriptor& descriptor, const FilePath& fileToConvert, eGPUFamily gpuFamily, TextureConverter::eConvertQuality quality, Vector<String>& args)
{
    DVASSERT(descriptor.compression);
    const TextureDescriptor::Compression* compression = &descriptor.compression[gpuFamily];

    String format = pixelFormatToPVRFormat[static_cast<PixelFormat>(compression->format)];
    FilePath outputFile = GetConvertedTexturePath(descriptor, gpuFamily);

    // input file
    args.push_back("-i");
    String inputName = GenerateInputName(descriptor, fileToConvert);
    args.push_back(inputName);

    // output file
    args.push_back("-o");
    args.push_back(outputFile.GetAbsolutePathname());

    // flip for some TGA files
    // PVR converter seems to have a bug:
    // if source file is a top-left TGA, then the resulting file will be flipped vertically.
    // to fix this, -flip param will be passed for such files
    if (descriptor.dataSettings.sourceFileFormat == IMAGE_FORMAT_TGA)
    {
        LibTgaHelper tgaHelper;
        LibTgaHelper::TgaInfo tgaInfo;
        auto readRes = tgaHelper.ReadTgaHeader(inputName, tgaInfo);
        if (readRes == DAVA::eErrorCode::SUCCESS)
        {
            switch (tgaInfo.origin_corner)
            {
            case LibTgaHelper::TgaInfo::BOTTOM_LEFT:
                break;
            case LibTgaHelper::TgaInfo::BOTTOM_RIGHT:
                args.push_back("-flip");
                args.push_back("x");
                break;
            case LibTgaHelper::TgaInfo::TOP_LEFT:
                args.push_back("-flip");
                args.push_back("y");
                break;
            case LibTgaHelper::TgaInfo::TOP_RIGHT:
                args.push_back("-flip");
                args.push_back("x");
                args.push_back("-flip");
                args.push_back("y");
                break;
            }
        }
        else
        {
            Logger::Error("Failed to read %s: error %d", inputName.c_str(), readRes);
        }
    }

    //quality
    args.push_back("-q");
    if (FORMAT_ETC1 == descriptor.compression[gpuFamily].format)
    {
        args.push_back(ETC_QUALITY_SETTING[quality]);
    }
    else
    {
        args.push_back(PVR_QUALITY_SETTING[quality]);
    }

    // mipmaps
    if (descriptor.GetGenerateMipMaps())
    {
        args.push_back("-m");
    }

    if (descriptor.IsCubeMap())
    {
        args.push_back("-cube");
    }

    // output format
    args.push_back("-f");
    args.push_back(format);

    // base mipmap level (base resize)
    if (0 != compression->compressToWidth && compression->compressToHeight != 0)
    {
        args.push_back("-r");
        args.push_back(Format("%d,%d", compression->compressToWidth, compression->compressToHeight));
    }

    //args.push_back("-l"); //Alpha Bleed: Discards any data in fully transparent areas to optimise the texture for better compression.
}

FilePath PVRConverter::GetConvertedTexturePath(const TextureDescriptor& descriptor, eGPUFamily gpuFamily)
{
    return descriptor.CreateMultiMipPathnameForGPU(gpuFamily);
}

void PVRConverter::SetPVRTexTool(const FilePath& textToolPathname)
{
    pvrTexToolPathname = textToolPathname;

    if (!FileSystem::Instance()->IsFile(pvrTexToolPathname))
    {
        Logger::Error("PVRTexTool doesn't found in %s\n", pvrTexToolPathname.GetAbsolutePathname().c_str());
        pvrTexToolPathname = FilePath();
    }
}

FilePath PVRConverter::PrepareCubeMapForPvrConvert(const TextureDescriptor& descriptor)
{
    DAVA::Vector<DAVA::FilePath> pvrToolFaceNames;
    DAVA::Vector<DAVA::FilePath> cubemapFaceNames;
    descriptor.GenerateFacePathnames(CUBEMAP_TMP_DIR, PVRTOOL_FACE_SUFFIXES, pvrToolFaceNames);
    descriptor.GetFacePathnames(cubemapFaceNames);

    if (!FileSystem::Instance()->IsDirectory(CUBEMAP_TMP_DIR))
    {
        int createResult = FileSystem::Instance()->CreateDirectory(CUBEMAP_TMP_DIR);
        if (FileSystem::DIRECTORY_CREATED != createResult)
        {
            DAVA::Logger::Error("Failed to create temp dir for cubemap generation!");
            return FilePath();
        }
    }

    for (size_t i = 0; i < pvrToolFaceNames.size(); ++i)
    {
        //cleanup in case previous cleanup failed
        if (FileSystem::Instance()->IsFile(pvrToolFaceNames[i]))
        {
            FileSystem::Instance()->DeleteFile(pvrToolFaceNames[i]);
        }

        bool result = FileSystem::Instance()->CopyFile(cubemapFaceNames[i], pvrToolFaceNames[i]);
        if (!result)
        {
            DAVA::Logger::Error("Failed to copy tmp files for cubemap generation!");
            return FilePath();
        }
    }

    return FilePath(pvrToolFaceNames[0]);
}

void PVRConverter::CleanupCubemapAfterConversion(const TextureDescriptor& descriptor)
{
    Vector<FilePath> pvrToolFaceNames;
    descriptor.GenerateFacePathnames(CUBEMAP_TMP_DIR, PVRTOOL_FACE_SUFFIXES, pvrToolFaceNames);

    for (auto& faceName : pvrToolFaceNames)
    {
        if (FileSystem::Instance()->IsFile(faceName))
        {
            FileSystem::Instance()->DeleteFile(faceName);
        }
    }
}

DAVA::String PVRConverter::GenerateInputName(const TextureDescriptor& descriptor, const FilePath& fileToConvert)
{
    if (descriptor.IsCubeMap())
    {
        Vector<FilePath> pvrToolFaceNames;
        descriptor.GenerateFacePathnames(CUBEMAP_TMP_DIR, PVRTOOL_FACE_SUFFIXES, pvrToolFaceNames);

        String retNames;
        for (size_t i = 0; i < pvrToolFaceNames.size(); ++i)
        {
            if (i)
            {
                retNames += ",";
            }

            retNames += pvrToolFaceNames[i].GetAbsolutePathname();
        }

        return retNames;
    }

    return fileToConvert.GetAbsolutePathname();
}
};
