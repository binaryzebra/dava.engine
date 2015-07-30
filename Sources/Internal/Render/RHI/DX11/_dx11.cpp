//==============================================================================
//
//  DX11 related stuff
//
//==============================================================================
//
//  externals:

    #include "_dx11.h"

    #include <stdio.h>

    #include "../rhi_Public.h"


//==============================================================================

namespace rhi
{

ID3D11Device*           _D3D11_Device               = nullptr;
IDXGISwapChain*         _D3D11_SwapChain            = nullptr;
ID3D11Texture2D*        _D3D11_SwapChainBuffer      = nullptr;
ID3D11RenderTargetView* _D3D11_RenderTargetView     = nullptr;
ID3D11Texture2D*        _D3D11_DepthStencilBuffer   = nullptr;
ID3D11DepthStencilView* _D3D11_DepthStencilView     = nullptr;
D3D_FEATURE_LEVEL       _D3D11_FeatureLevel         = D3D_FEATURE_LEVEL_9_1;
ID3D11DeviceContext*    _D3D11_ImmediateContext     = nullptr;
ID3D11Debug*            _D3D11_Debug                = nullptr;

InitParam               _DX11_InitParam;

//-void(*_End_Frame)() = nullptr;

}


//==============================================================================
//
//  publics:

const char*
D3D11ErrorText( HRESULT hr )
{
    switch( hr )
    {
        case D3D_OK :
            return "No error occurred";

        case D3DOK_NOAUTOGEN :
            return "This is a success code. However, the autogeneration of mipmaps is not supported for this format. This means that resource creation will succeed but the mipmap levels will not be automatically generated";

        case D3DERR_CONFLICTINGRENDERSTATE :
            return "The currently set render states cannot be used together";

        case D3DERR_CONFLICTINGTEXTUREFILTER :
            return "The current texture filters cannot be used together";

        case D3DERR_CONFLICTINGTEXTUREPALETTE :
            return "The current textures cannot be used simultaneously";

        case D3DERR_DEVICELOST :
            return "The device has been lost but cannot be reset at this time. Therefore, rendering is not possible";

        case D3DERR_DEVICENOTRESET :
            return "The device has been lost but can be reset at this time";

        case D3DERR_DRIVERINTERNALERROR :
            return "Internal driver error. Applications should destroy and recreate the device when receiving this error. For hints on debugging this error, see Driver Internal Errors";

        case D3DERR_DRIVERINVALIDCALL :
            return "Not used";

        case D3DERR_INVALIDCALL :
            return "The method call is invalid. For example, a method's parameter may not be a valid pointer";

        case D3DERR_INVALIDDEVICE :
            return "The requested device type is not valid";

        case D3DERR_MOREDATA :
            return "There is more data available than the specified buffer size can hold";

        case D3DERR_NOTAVAILABLE :
            return "This device does not support the queried technique";

        case D3DERR_NOTFOUND :
            return "The requested item was not found";

        case D3DERR_OUTOFVIDEOMEMORY :
            return "Direct3D does not have enough display memory to perform the operation";

        case D3DERR_TOOMANYOPERATIONS :
            return "The application is requesting more texture-filtering operations than the device supports";

        case D3DERR_UNSUPPORTEDALPHAARG :
            return "The device does not support a specified texture-blending argument for the alpha channel";

        case D3DERR_UNSUPPORTEDALPHAOPERATION :
            return "The device does not support a specified texture-blending operation for the alpha channel";

        case D3DERR_UNSUPPORTEDCOLORARG :
            return "The device does not support a specified texture-blending argument for color values";

        case D3DERR_UNSUPPORTEDCOLOROPERATION :
            return "The device does not support a specified texture-blending operation for color values";

        case D3DERR_UNSUPPORTEDFACTORVALUE :
            return "The device does not support the specified texture factor value. Not used; provided only to support older drivers";

        case D3DERR_UNSUPPORTEDTEXTUREFILTER :
            return "The device does not support the specified texture filter";

        case D3DERR_WASSTILLDRAWING :
            return "The previous blit operation that is transferring information to or from this surface is incomplete";

        case D3DERR_WRONGTEXTUREFORMAT :
            return "The pixel format of the texture surface is not valid";

        case E_FAIL :
            return "An undetermined error occurred inside the Direct3D subsystem";

        case E_INVALIDARG :
            return "An invalid parameter was passed to the returning function";

//        case E_INVALIDCALL :
//            return "The method call is invalid. For example, a method's parameter may have an invalid value";

        case E_NOINTERFACE :
            return "No object interface is available";

        case E_NOTIMPL :
            return "Not implemented";

        case E_OUTOFMEMORY :
            return "Direct3D could not allocate sufficient memory to complete the call";

    }

    static char text[1024];

    _snprintf( text, sizeof(text),"unknown D3D9 error (%08X)\n", (unsigned)hr );
    return text;
}

namespace rhi
{

//------------------------------------------------------------------------------

DXGI_FORMAT
DX11_TextureFormat( TextureFormat format )
{
    switch( format )
    {
        case TEXTURE_FORMAT_R8G8B8A8        : return DXGI_FORMAT_B8G8R8A8_UNORM;
        case TEXTURE_FORMAT_R8G8B8X8        : return DXGI_FORMAT_B8G8R8X8_UNORM;
//        case TEXTURE_FORMAT_R8G8B8          : return DXGI_FORMAT_R8G8B8_UNORM;

        case TEXTURE_FORMAT_R5G5B5A1        : return DXGI_FORMAT_B5G5R5A1_UNORM;
        case TEXTURE_FORMAT_R5G6B5          : return DXGI_FORMAT_B5G6R5_UNORM;

//        case TEXTURE_FORMAT_R4G4B4A4        : return DXGI_FORMAT_B4G4R4A4_UNORM;

//        case TEXTURE_FORMAT_A16R16G16B16    : return D3DFMT_A16B16G16R16F;
//        case TEXTURE_FORMAT_A32R32G32B32    : return D3DFMT_A32B32G32R32F;

        case TEXTURE_FORMAT_R8              : return DXGI_FORMAT_A8_UNORM;
        case TEXTURE_FORMAT_R16             : return DXGI_FORMAT_R16_FLOAT;

        case TEXTURE_FORMAT_DXT1            : return DXGI_FORMAT_BC1_UNORM;
        case TEXTURE_FORMAT_DXT3            : return DXGI_FORMAT_BC4_UNORM;
        case TEXTURE_FORMAT_DXT5            : return DXGI_FORMAT_BC2_UNORM;

        case TEXTURE_FORMAT_PVRTC2_4BPP_RGB :
        case TEXTURE_FORMAT_PVRTC2_4BPP_RGBA :
        case TEXTURE_FORMAT_PVRTC2_2BPP_RGB :
        case TEXTURE_FORMAT_PVRTC2_2BPP_RGBA :
            return DXGI_FORMAT_UNKNOWN;

        case TEXTURE_FORMAT_ATC_RGB :
        case TEXTURE_FORMAT_ATC_RGBA_EXPLICIT :
        case TEXTURE_FORMAT_ATC_RGBA_INTERPOLATED :
            return DXGI_FORMAT_UNKNOWN;

        case TEXTURE_FORMAT_ETC1 :
        case TEXTURE_FORMAT_ETC2_R8G8B8 :
        case TEXTURE_FORMAT_ETC2_R8G8B8A8 :
        case TEXTURE_FORMAT_ETC2_R8G8B8A1 :
            return DXGI_FORMAT_UNKNOWN;

        case TEXTURE_FORMAT_EAC_R11_UNSIGNED :
        case TEXTURE_FORMAT_EAC_R11_SIGNED :
        case TEXTURE_FORMAT_EAC_R11G11_UNSIGNED :
        case TEXTURE_FORMAT_EAC_R11G11_SIGNED :
            return DXGI_FORMAT_UNKNOWN;

        case TEXTURE_FORMAT_D16             : return DXGI_FORMAT_D16_UNORM;
        case TEXTURE_FORMAT_D24S8           : return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }

    return DXGI_FORMAT_UNKNOWN;
}



} // namespace rhi
