#pragma once

struct CaptureFrame
{
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface FrameTexture;
    winrt::Windows::Graphics::SizeInt32 ContentSize;
    winrt::Windows::Foundation::TimeSpan SystemRelativeTime;
    long long current_frame_seq = 0;
};

class SimpleVideoEncoder
{
public:
    SimpleVideoEncoder(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
        winrt::Windows::Graphics::SizeInt32 const& resolution,
        uint32_t bitRate,
        uint32_t frameRate
    );
    ~SimpleVideoEncoder();


    winrt::Windows::Foundation::IAsyncAction StartEncoder();



    void CreateMediaObjects();
    void OnMediaStreamSourceStarting(winrt::Windows::Media::Core::MediaStreamSource const& sender, winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);
    void OnMediaStreamSourceSampleRequested(winrt::Windows::Media::Core::MediaStreamSource const& sender, winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);

    winrt::Windows::Foundation::IAsyncAction EncodeAsync(winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame src_frame);
    //winrt::Windows::Foundation::IAsyncAction EncodeAsync(winrt::Windows::Storage::Streams::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate);
    //winrt::Windows::Foundation::IAsyncAction EncodeInternalAsync(winrt::Windows::Storage::Streams::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate);

    void StrartReadThread();
    winrt::Windows::Foundation::IAsyncAction StreamReadAsync();

    //void GetFrame(winrt::IDirect3DSurface const& surface);

    //enum class SupportedFormats
    //{
    //    Png,
    //    Jpg,
    //    Jxr
    //};

    //void EncodeImage(
    //    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface,
    //    winrt::Windows::Storage::Streams::IRandomAccessStream const& stream,
    //    SupportedFormats const& format = SupportedFormats::Png);

public:
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<ID3D11Device> m_d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;

    std::optional<CaptureFrame> m_capture_frame;
    winrt::Windows::Storage::Streams::InMemoryRandomAccessStream m_memory_stream{ nullptr };
    winrt::Windows::Storage::Streams::DataReader m_memory_stream_reader{ nullptr };
    uint32_t read_size = 0;
    long long frame_seq = 0;

    //winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame m_currentFrame{ nullptr };

    //winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };

    winrt::Windows::Media::Core::VideoStreamDescriptor m_videoDescriptor{ nullptr };
    winrt::Windows::Media::MediaProperties::MediaEncodingProfile m_encodingProfile{ nullptr };

    winrt::Windows::Media::Core::MediaStreamSource m_streamSource{ nullptr };
    winrt::Windows::Media::Transcoding::MediaTranscoder m_transcoder{ nullptr };

    //std::atomic<bool> m_is_encoder_start = false;
    //private CaptureFrameWait _frameGenerator;



    //winrt::com_ptr<ID2D1Factory1> m_d2dFactory;
    //winrt::com_ptr<ID2D1Device> m_d2dDevice;
    //winrt::com_ptr<ID2D1DeviceContext> m_d2dContext;
    //winrt::com_ptr<IWICImagingFactory2> m_wicFactory;

    wil::shared_event m_nextFrameEvent;
};