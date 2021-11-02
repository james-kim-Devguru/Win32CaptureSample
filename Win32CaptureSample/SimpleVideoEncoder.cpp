#include "pch.h"
#include "SimpleVideoEncoder.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;

    using namespace Windows::Media;
    using namespace Windows::Media::Core;
    using namespace Windows::Media::MediaProperties;
    using namespace Windows::Media::Transcoding;
}

namespace util
{
    using namespace uwp;
}

SimpleVideoEncoder::SimpleVideoEncoder(winrt::IDirect3DDevice const& device, winrt::GraphicsCaptureItem const& item)
{
    m_device = device;
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    m_item = item;
    m_isRecording = false;

    CreateMediaObjects();
}

void SimpleVideoEncoder::CreateMediaObjects()
{
    // Create our encoding profile based on the size of the item
    int width = m_item.Size().Width;
    int height = m_item.Size().Height;

    // Describe our input: uncompressed BGRA8 buffers
    auto videoProperties = winrt::VideoEncodingProperties().CreateUncompressed(winrt::MediaEncodingSubtypes::Bgra8(), width, height);
    m_videoDescriptor = winrt::VideoStreamDescriptor(videoProperties);


    // Create our MediaStreamSource
    m_mediaStreamSource = winrt::MediaStreamSource(m_videoDescriptor);
    m_mediaStreamSource.BufferTime(winrt::TimeSpan(0));
    m_mediaStreamSource.Starting({ this, &SimpleVideoEncoder::OnMediaStreamSourceStarting });
    m_mediaStreamSource.SampleRequested({ this, &SimpleVideoEncoder::OnMediaStreamSourceSampleRequested });


    // Create our transcoder
    m_transcoder = winrt::Windows::Media::Transcoding::MediaTranscoder();
    m_transcoder.HardwareAccelerationEnabled(true);
}

void SimpleVideoEncoder::OnMediaStreamSourceStarting(winrt::MediaStreamSource const& sender, winrt::MediaStreamSourceStartingEventArgs const& args)
{

    //using (var frame = _frameGenerator.WaitForNewFrame())
    //{
    //    args.Request.SetActualStartPosition(frame.SystemRelativeTime);
    //}
}

void SimpleVideoEncoder::OnMediaStreamSourceSampleRequested(winrt::MediaStreamSource const& sender, winrt::MediaStreamSourceSampleRequestedEventArgs const& args)
{
    //if (_isRecording && !_closed)
    //{
    //    try
    //    {
    //        using (var frame = _frameGenerator.WaitForNewFrame())
    //        {
    //            if (frame == null)
    //            {
    //                args.Request.Sample = null;
    //                DisposeInternal();
    //                return;
    //            }

    //            if (_isPreviewing)
    //            {
    //                lock(_previewLock)
    //                {
    //                    _preview.PresentSurface(frame.Surface);
    //                }
    //            }

    //            var timeStamp = frame.SystemRelativeTime;
    //            var sample = MediaStreamSample.CreateFromDirect3D11Surface(frame.Surface, timeStamp);
    //            args.Request.Sample = sample;
    //        }
    //    }
    //    catch (Exception e)
    //    {
    //        Debug.WriteLine(e.Message);
    //        Debug.WriteLine(e.StackTrace);
    //        Debug.WriteLine(e);
    //        args.Request.Sample = null;
    //        DisposeInternal();
    //    }
    //}
    //else
    //{
    //    args.Request.Sample = null;
    //    DisposeInternal();
    //}
}



winrt::IAsyncAction SimpleVideoEncoder::EncodeAsync(winrt::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate)
{
    //auto abiStream = util::CreateStreamFromRandomAccessStream(stream);

    if (!m_isRecording)
    {
        m_isRecording = true;

        //_frameGenerator = new CaptureFrameWait(
        //    _device,
        //    _captureItem,
        //    _captureItem.Size);

        //using (_frameGenerator)
        //{
        auto encodingProfile = winrt::MediaEncodingProfile();
        encodingProfile.Container().Subtype(L"MPEG4");
        encodingProfile.Video().Subtype(L"H264");
        encodingProfile.Video().Width(width);
        encodingProfile.Video().Height(height);
        encodingProfile.Video().Bitrate(bitrateInBps);
        encodingProfile.Video().FrameRate().Numerator(frameRate);
        encodingProfile.Video().FrameRate().Denominator(1);
        encodingProfile.Video().PixelAspectRatio().Numerator(1);
        encodingProfile.Video().PixelAspectRatio().Denominator(1);
        auto transcode = co_await m_transcoder.PrepareMediaStreamSourceTranscodeAsync(m_mediaStreamSource, stream, encodingProfile);

        co_await transcode.TranscodeAsync();
        //}
    }
}

void SimpleVideoEncoder::GetFrame(winrt::IDirect3DSurface const& surface)
{
    // Get the texture and setup the D2D bitmap
    auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(surface);

    //D3D11_TEXTURE2D_DESC textureDesc = {};
    //frameTexture->GetDesc(&textureDesc);
    //auto dxgiFrameTexture = frameTexture.as<IDXGISurface>();


    auto width = Math.Clamp(_currentFrame.ContentSize.Width, 0, _currentFrame.Surface.Description.Width);
    auto height = Math.Clamp(_currentFrame.ContentSize.Height, 0, _currentFrame.Surface.Description.Height);
    auto region = new SharpDX.Direct3D11.ResourceRegion(0, 0, 0, width, height, 1);



    

    _d3dDevice.ImmediateContext.ClearRenderTargetView(_composeRenderTargetView, new SharpDX.Mathematics.Interop.RawColor4(0, 0, 0, 1));

        var width = Math.Clamp(_currentFrame.ContentSize.Width, 0, _currentFrame.Surface.Description.Width);
        var height = Math.Clamp(_currentFrame.ContentSize.Height, 0, _currentFrame.Surface.Description.Height);
        var region = new SharpDX.Direct3D11.ResourceRegion(0, 0, 0, width, height, 1);
        _d3dDevice.ImmediateContext.CopySubresourceRegion(sourceTexture, 0, region, _composeTexture, 0);

        var description = sourceTexture.Description;
        description.Usage = SharpDX.Direct3D11.ResourceUsage.Default;
        description.BindFlags = SharpDX.Direct3D11.BindFlags.ShaderResource | SharpDX.Direct3D11.BindFlags.RenderTarget;
        description.CpuAccessFlags = SharpDX.Direct3D11.CpuAccessFlags.None;
        description.OptionFlags = SharpDX.Direct3D11.ResourceOptionFlags.None;

        using (var copyTexture = new SharpDX.Direct3D11.Texture2D(_d3dDevice, description))
        {
            _d3dDevice.ImmediateContext.CopyResource(_composeTexture, copyTexture);
            result.Surface = Direct3D11Helpers.CreateDirect3DSurfaceFromSharpDXTexture(copyTexture);
        }
    //}





        // TODO: Since this sample doesn't use D2D any other way, it may be better to map 
        //       the pixels manually and hand them to WIC. However, using d2d is easier for now.
        winrt::com_ptr<ID2D1Bitmap1> d2dBitmap;
        winrt::check_hresult(m_d2dContext->CreateBitmapFromDxgiSurface(dxgiFrameTexture.get(), nullptr, d2dBitmap.put()));

        // Get the WIC settings for the given format
        auto wicSettings = GetWICSettingsForFormat(format);

        // Encode the image
        // TODO: dpi?
        auto dpi = 96.0f;
        WICImageParameters params = {};
        params.PixelFormat.format = textureDesc.Format;
        params.PixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        params.DpiX = dpi;
        params.DpiY = dpi;
        params.PixelWidth = textureDesc.Width;
        params.PixelHeight = textureDesc.Height;

        winrt::com_ptr<IWICBitmapEncoder> encoder;
        winrt::check_hresult(m_wicFactory->CreateEncoder(wicSettings.ContainerFormat, nullptr, encoder.put()));
        winrt::check_hresult(encoder->Initialize(abiStream.get(), WICBitmapEncoderNoCache));

        winrt::com_ptr<IWICBitmapFrameEncode> wicFrame;
        winrt::com_ptr<IPropertyBag2> frameProperties;
        winrt::check_hresult(encoder->CreateNewFrame(wicFrame.put(), frameProperties.put()));
        winrt::check_hresult(wicFrame->Initialize(frameProperties.get()));
        auto wicPixelFormat = wicSettings.PixelFormat;
        winrt::check_hresult(wicFrame->SetPixelFormat(&wicPixelFormat));

        winrt::com_ptr<IWICImageEncoder> imageEncoder;
        winrt::check_hresult(m_wicFactory->CreateImageEncoder(m_d2dDevice.get(), imageEncoder.put()));
        winrt::check_hresult(imageEncoder->WriteFrame(d2dBitmap.get(), wicFrame.get(), &params));
        winrt::check_hresult(wicFrame->Commit());
        winrt::check_hresult(encoder->Commit());
}


//winrt::IAsyncAction SimpleVideoEncoder::EncodeInternalAsync(winrt::Windows::Storage::Streams::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate)
//{
//
//}