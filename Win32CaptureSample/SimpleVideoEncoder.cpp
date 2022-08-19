#include "pch.h"
#include "SimpleVideoEncoder.h"
#include <robmikh.common/d3dHelpers.h>

#include <thread>


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

//namespace util
//{
//    using namespace uwp;
//}

int32_t EnsureEven(int32_t value)
{
    if (value % 2 == 0)
    {
        return value;
    }
    else
    {
        return value + 1;
    }
}

SimpleVideoEncoder::SimpleVideoEncoder(
    winrt::Direct3D11CaptureFrame frame,
    winrt::GraphicsCaptureItem const& item,
    winrt::SizeInt32 const& resolution,
    uint32_t bitRate,
    uint32_t frameRate
)
{
    auto d3dDevice = robmikh::common::uwp::CreateD3DDevice();
    auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    m_device = CreateDirect3DDevice(dxgiDevice.get());
    m_d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    m_d3dDevice->GetImmediateContext(m_d3dContext.put());

    m_item = item;
    m_capture_frame = std::optional<CaptureFrame>({
        frame.Surface(),
        frame.ContentSize(),
        frame.SystemRelativeTime(),
    });

    auto itemSize = item.Size();
    auto inputWidth = EnsureEven(itemSize.Width);
    auto inputHeight = EnsureEven(itemSize.Height);
    auto outputWidth = EnsureEven(resolution.Width);
    auto outputHeight = EnsureEven(resolution.Height);

    // Describe out output: H264 video with an MP4 container
    m_encodingProfile = winrt::MediaEncodingProfile();
    m_encodingProfile.Container().Subtype(L"MPEG4");
    auto video = m_encodingProfile.Video();
    video.Subtype(L"H264");
    video.Width(outputWidth);
    video.Height(outputHeight);
    video.Bitrate(bitRate);
    video.FrameRate().Numerator(frameRate);
    video.FrameRate().Denominator(1);
    video.PixelAspectRatio().Numerator(1);
    video.PixelAspectRatio().Denominator(1);
    m_encodingProfile.Video(video);

    // Describe our input: uncompressed BGRA8 buffers
    auto properties = winrt::VideoEncodingProperties::CreateUncompressed(
        winrt::MediaEncodingSubtypes::Bgra8(),
        static_cast<uint32_t>(inputWidth),
        static_cast<uint32_t>(inputHeight));
    m_videoDescriptor = winrt::VideoStreamDescriptor(properties);

    m_memory_stream = winrt::InMemoryRandomAccessStream();
    m_memory_stream_reader = winrt::DataReader(m_memory_stream.GetInputStreamAt(0));
    m_memory_stream_reader.InputStreamOptions(winrt::InputStreamOptions::ReadAhead);

    m_nextFrameEvent = wil::shared_event(wil::EventOptions::ManualReset);
}


SimpleVideoEncoder::~SimpleVideoEncoder()
{
}

winrt::IAsyncAction SimpleVideoEncoder::StartEncoder()
{
    /*auto expected = false;
    if (m_is_encoder_start.compare_exchange_strong(expected, true))
    {*/
        // Create our MediaStreamSource
        m_streamSource = winrt::MediaStreamSource(m_videoDescriptor);
        m_streamSource.BufferTime(std::chrono::seconds(0));
        m_streamSource.Starting({ this, &SimpleVideoEncoder::OnMediaStreamSourceStarting });
        m_streamSource.SampleRequested({ this, &SimpleVideoEncoder::OnMediaStreamSourceSampleRequested });

        // Create our transcoder
        m_transcoder = winrt::MediaTranscoder();
        m_transcoder.HardwareAccelerationEnabled(true);
        m_transcoder.AlwaysReencode(true);

        // Start encoding
        auto transcode = co_await m_transcoder.PrepareMediaStreamSourceTranscodeAsync(m_streamSource, m_memory_stream, m_encodingProfile);

        co_await transcode.TranscodeAsync();
        //winrt::IAsyncActionWithProgress<double> transcode_progress = transcode.TranscodeAsync();
    //}
    co_return;
}

//
//void SimpleVideoEncoder::CreateMediaObjects()
//{
//    // Create our encoding profile based on the size of the item
//    int width = m_item.Size().Width;
//    int height = m_item.Size().Height;
//
//    // Describe our input: uncompressed BGRA8 buffers
//    auto videoProperties = winrt::VideoEncodingProperties().CreateUncompressed(winrt::MediaEncodingSubtypes::Bgra8(), width, height);
//    m_videoDescriptor = winrt::VideoStreamDescriptor(videoProperties);
//
//
//    // Create our MediaStreamSource
//    m_mediaStreamSource = winrt::MediaStreamSource(m_videoDescriptor);
//    m_mediaStreamSource.BufferTime(winrt::TimeSpan(0));
//    m_mediaStreamSource.Starting({ this, &SimpleVideoEncoder::OnMediaStreamSourceStarting });
//    m_mediaStreamSource.SampleRequested({ this, &SimpleVideoEncoder::OnMediaStreamSourceSampleRequested });
//
//
//    // Create our transcoder
//    m_transcoder = winrt::Windows::Media::Transcoding::MediaTranscoder();
//    m_transcoder.HardwareAccelerationEnabled(true);
//}
//
void SimpleVideoEncoder::OnMediaStreamSourceStarting(winrt::MediaStreamSource const& sender, winrt::MediaStreamSourceStartingEventArgs const& args)
{
    if(m_capture_frame)
        args.Request().SetActualStartPosition(m_capture_frame->SystemRelativeTime);
}

void SimpleVideoEncoder::OnMediaStreamSourceSampleRequested(winrt::MediaStreamSource const& sender, winrt::MediaStreamSourceSampleRequestedEventArgs const& args)
{
    auto request = args.Request();

    try
    {

        winrt::TimeSpan timeStamp;
        winrt::SizeInt32 contentSize;
        winrt::com_ptr<ID3D11Texture2D> frameTexture;
        long long current_frame_seq = 0;

        while (true)
        {
            try
            {

                if (m_capture_frame->FrameTexture != nullptr)
                {
                    m_capture_frame->FrameTexture.Close();
                }
                m_nextFrameEvent.ResetEvent();
                HANDLE next_frame_event = m_nextFrameEvent.get();
                auto waitResult = WaitForSingleObjectEx(next_frame_event, INFINITE, false);

                timeStamp = m_capture_frame->SystemRelativeTime;
                contentSize = m_capture_frame->ContentSize;
                frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(m_capture_frame->FrameTexture);
                current_frame_seq = m_capture_frame->current_frame_seq;

                break;
            }
            catch (winrt::hresult_error const& error)
            {
                continue;
            }
        }

        std::wstring log_msg;
        log_msg = std::wstring(L"OnMediaStreamSourceSampleRequested frame seq") + std::to_wstring(current_frame_seq) + std::wstring(L"\n");
        OutputDebugStringW(log_msg.c_str());


        //D3D11_TEXTURE2D_DESC desc = {};
        //frameTexture->GetDesc(&desc);

        //// CopyResource can fail if our new texture isn't the same size as the back buffer
        //// TODO: Fix how resolutions are handled
        //desc = {};
        //backBuffer->GetDesc(&desc);

        //desc.Usage = D3D11_USAGE_DEFAULT;
        //desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        //desc.CPUAccessFlags = 0;
        //desc.MiscFlags = 0;
        //winrt::com_ptr<ID3D11Texture2D> sampleTexture;
        //winrt::check_hresult(m_d3dDevice->CreateTexture2D(&desc, nullptr, sampleTexture.put()));
        //m_d3dContext->CopyResource(sampleTexture.get(), frameTexture.get());
        //auto dxgiSurface = sampleTexture.as<IDXGISurface>();
        //auto sampleSurface = CreateDirect3DSurface(dxgiSurface.get());

        //DXGI_PRESENT_PARAMETERS presentParameters{};
        //winrt::check_hresult(m_previewSwapChain->Present1(0, 0, &presentParameters));

        D3D11_TEXTURE2D_DESC desc = {};
        frameTexture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        winrt::com_ptr<ID3D11Texture2D> sampleTexture;
        winrt::check_hresult(m_d3dDevice->CreateTexture2D(&desc, nullptr, sampleTexture.put()));
        m_d3dContext->CopyResource(sampleTexture.get(), frameTexture.get());
        auto dxgiSurface = sampleTexture.as<IDXGISurface>();
        auto sampleSurface = CreateDirect3DSurface(dxgiSurface.get());

        auto sample = winrt::MediaStreamSample::CreateFromDirect3D11Surface(sampleSurface, timeStamp);
        request.Sample(sample);
    }
    catch (winrt::hresult_error const& error)
    {
        OutputDebugStringW(error.message().c_str());
        request.Sample(nullptr);
        //CloseInternal();
        return;
    }
}

winrt::IAsyncAction SimpleVideoEncoder::EncodeAsync(winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame src_frame)
{
    m_capture_frame = std::optional<CaptureFrame>({
        src_frame.Surface(),
        src_frame.ContentSize(),
        src_frame.SystemRelativeTime(),
        ++frame_seq
        });

    std::wstring log_msg;

    log_msg = std::wstring(L"EncodeAsync frame seq") + std::to_wstring(frame_seq) + std::wstring(L"\n");
    OutputDebugStringW(log_msg.c_str());

    m_nextFrameEvent.SetEvent();

    //if (m_memory_stream.Size() == 0)
    //    return;

    //while (true)
    //{
    //    if (m_memory_stream.Size() == 0)
    //    {
    //        Sleep(10);
    //        continue;
    //    }
    //    else
    //        break;        
    //}

    uint32_t stream_size = (uint32_t)m_memory_stream.Size();

    log_msg = std::wstring(L"stream position: ") + std::to_wstring(m_memory_stream.Position()) + std::wstring(L"\n");
    OutputDebugStringW(log_msg.c_str());

    //log_msg = std::wstring(L"stream_size : ") + std::to_wstring(stream_size) + std::wstring(L"\n");
    //OutputDebugStringW(log_msg.c_str());
    if (m_memory_stream.CanRead())
    {
        auto return_size = co_await m_memory_stream_reader.LoadAsync(stream_size - read_size);
        read_size += return_size;
        log_msg = std::wstring(L"read stream_size : ") + std::to_wstring(return_size) + std::wstring(L"\n");
        OutputDebugStringW(log_msg.c_str());
        std::vector<unsigned char> buffer(return_size);
        m_memory_stream_reader.ReadBytes(buffer);
        //co_await m_memory_stream.FlushAsync();
    }

    co_return;
}

void SimpleVideoEncoder::StrartReadThread()
{
    
}

winrt::IAsyncAction SimpleVideoEncoder::StreamReadAsync()
{
    std::wstring log_msg;
        
    uint32_t stream_size = (uint32_t)this->m_memory_stream.Size();

    log_msg = std::wstring(L"stream position: ") + std::to_wstring(this->m_memory_stream.Position()) + std::wstring(L"\n");
    OutputDebugStringW(log_msg.c_str());

    //log_msg = std::wstring(L"stream_size : ") + std::to_wstring(stream_size) + std::wstring(L"\n");
    //OutputDebugStringW(log_msg.c_str());
    if (this->m_memory_stream.CanRead())
    {
        auto return_size = co_await this->m_memory_stream_reader.LoadAsync(stream_size - this->read_size);
        this->read_size += return_size;
        log_msg = std::wstring(L"read stream_size : ") + std::to_wstring(return_size) + std::wstring(L"\n");
        OutputDebugStringW(log_msg.c_str());
        std::vector<unsigned char> buffer(return_size);
        m_memory_stream_reader.ReadBytes(buffer);
        //co_await m_memory_stream.FlushAsync();
    }

    co_return;
}




//
//
//
//winrt::IAsyncAction SimpleVideoEncoder::EncodeAsync(winrt::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate)
//{
//    //auto abiStream = util::CreateStreamFromRandomAccessStream(stream);
//
//    if (!m_isRecording)
//    {
//        m_isRecording = true;
//
//        //_frameGenerator = new CaptureFrameWait(
//        //    _device,
//        //    _captureItem,
//        //    _captureItem.Size);
//
//        //using (_frameGenerator)
//        //{
//        auto encodingProfile = winrt::MediaEncodingProfile();
//        encodingProfile.Container().Subtype(L"MPEG4");
//        encodingProfile.Video().Subtype(L"H264");
//        encodingProfile.Video().Width(width);
//        encodingProfile.Video().Height(height);
//        encodingProfile.Video().Bitrate(bitrateInBps);
//        encodingProfile.Video().FrameRate().Numerator(frameRate);
//        encodingProfile.Video().FrameRate().Denominator(1);
//        encodingProfile.Video().PixelAspectRatio().Numerator(1);
//        encodingProfile.Video().PixelAspectRatio().Denominator(1);
//        auto transcode = co_await m_transcoder.PrepareMediaStreamSourceTranscodeAsync(m_mediaStreamSource, stream, encodingProfile);
//
//        co_await transcode.TranscodeAsync();
//        //}
//    }
//}
//
//void SimpleVideoEncoder::GetFrame(winrt::IDirect3DSurface const& surface)
//{
//    // Get the texture and setup the D2D bitmap
//    auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(surface);
//
//    //D3D11_TEXTURE2D_DESC textureDesc = {};
//    //frameTexture->GetDesc(&textureDesc);
//    //auto dxgiFrameTexture = frameTexture.as<IDXGISurface>();
//
//
//    auto width = Math.Clamp(_currentFrame.ContentSize.Width, 0, _currentFrame.Surface.Description.Width);
//    auto height = Math.Clamp(_currentFrame.ContentSize.Height, 0, _currentFrame.Surface.Description.Height);
//    auto region = new SharpDX.Direct3D11.ResourceRegion(0, 0, 0, width, height, 1);
//
//
//
//    
//
//    _d3dDevice.ImmediateContext.ClearRenderTargetView(_composeRenderTargetView, new SharpDX.Mathematics.Interop.RawColor4(0, 0, 0, 1));
//
//        var width = Math.Clamp(_currentFrame.ContentSize.Width, 0, _currentFrame.Surface.Description.Width);
//        var height = Math.Clamp(_currentFrame.ContentSize.Height, 0, _currentFrame.Surface.Description.Height);
//        var region = new SharpDX.Direct3D11.ResourceRegion(0, 0, 0, width, height, 1);
//        _d3dDevice.ImmediateContext.CopySubresourceRegion(sourceTexture, 0, region, _composeTexture, 0);
//
//        var description = sourceTexture.Description;
//        description.Usage = SharpDX.Direct3D11.ResourceUsage.Default;
//        description.BindFlags = SharpDX.Direct3D11.BindFlags.ShaderResource | SharpDX.Direct3D11.BindFlags.RenderTarget;
//        description.CpuAccessFlags = SharpDX.Direct3D11.CpuAccessFlags.None;
//        description.OptionFlags = SharpDX.Direct3D11.ResourceOptionFlags.None;
//
//        using (var copyTexture = new SharpDX.Direct3D11.Texture2D(_d3dDevice, description))
//        {
//            _d3dDevice.ImmediateContext.CopyResource(_composeTexture, copyTexture);
//            result.Surface = Direct3D11Helpers.CreateDirect3DSurfaceFromSharpDXTexture(copyTexture);
//        }
//    //}
//
//
//
//
//
//        // TODO: Since this sample doesn't use D2D any other way, it may be better to map 
//        //       the pixels manually and hand them to WIC. However, using d2d is easier for now.
//        winrt::com_ptr<ID2D1Bitmap1> d2dBitmap;
//        winrt::check_hresult(m_d2dContext->CreateBitmapFromDxgiSurface(dxgiFrameTexture.get(), nullptr, d2dBitmap.put()));
//
//        // Get the WIC settings for the given format
//        auto wicSettings = GetWICSettingsForFormat(format);
//
//        // Encode the image
//        // TODO: dpi?
//        auto dpi = 96.0f;
//        WICImageParameters params = {};
//        params.PixelFormat.format = textureDesc.Format;
//        params.PixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
//        params.DpiX = dpi;
//        params.DpiY = dpi;
//        params.PixelWidth = textureDesc.Width;
//        params.PixelHeight = textureDesc.Height;
//
//        winrt::com_ptr<IWICBitmapEncoder> encoder;
//        winrt::check_hresult(m_wicFactory->CreateEncoder(wicSettings.ContainerFormat, nullptr, encoder.put()));
//        winrt::check_hresult(encoder->Initialize(abiStream.get(), WICBitmapEncoderNoCache));
//
//        winrt::com_ptr<IWICBitmapFrameEncode> wicFrame;
//        winrt::com_ptr<IPropertyBag2> frameProperties;
//        winrt::check_hresult(encoder->CreateNewFrame(wicFrame.put(), frameProperties.put()));
//        winrt::check_hresult(wicFrame->Initialize(frameProperties.get()));
//        auto wicPixelFormat = wicSettings.PixelFormat;
//        winrt::check_hresult(wicFrame->SetPixelFormat(&wicPixelFormat));
//
//        winrt::com_ptr<IWICImageEncoder> imageEncoder;
//        winrt::check_hresult(m_wicFactory->CreateImageEncoder(m_d2dDevice.get(), imageEncoder.put()));
//        winrt::check_hresult(imageEncoder->WriteFrame(d2dBitmap.get(), wicFrame.get(), &params));
//        winrt::check_hresult(wicFrame->Commit());
//        winrt::check_hresult(encoder->Commit());
//}
//
//
////winrt::IAsyncAction SimpleVideoEncoder::EncodeInternalAsync(winrt::Windows::Storage::Streams::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate)
////{
////
////}