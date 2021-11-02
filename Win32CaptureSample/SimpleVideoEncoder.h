#pragma once

class SimpleVideoEncoder
{
public:
    SimpleVideoEncoder(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device, winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item);
    ~SimpleVideoEncoder() {}

    void CreateMediaObjects();
    void OnMediaStreamSourceStarting(winrt::Windows::Media::Core::MediaStreamSource const& sender, winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);
    void OnMediaStreamSourceSampleRequested(winrt::Windows::Media::Core::MediaStreamSource const& sender, winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);

    winrt::Windows::Foundation::IAsyncAction EncodeAsync(winrt::Windows::Storage::Streams::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate);
    //winrt::Windows::Foundation::IAsyncAction EncodeInternalAsync(winrt::Windows::Storage::Streams::IRandomAccessStream const& stream, UINT32 width, UINT32 height, UINT32 bitrateInBps, UINT32 frameRate);

    void GetFrame(winrt::IDirect3DSurface const& surface);






    enum class SupportedFormats
    {
        Png,
        Jpg,
        Jxr
    };

    void EncodeImage(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface,
        winrt::Windows::Storage::Streams::IRandomAccessStream const& stream,
        SupportedFormats const& format = SupportedFormats::Png);

private:
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };

    winrt::Windows::Media::Core::VideoStreamDescriptor m_videoDescriptor{ nullptr };
    winrt::Windows::Media::Core::MediaStreamSource m_mediaStreamSource{ nullptr };
    winrt::Windows::Media::Transcoding::MediaTranscoder m_transcoder{ nullptr };
    bool m_isRecording;
    //private CaptureFrameWait _frameGenerator;



    winrt::com_ptr<ID2D1Factory1> m_d2dFactory;
    winrt::com_ptr<ID2D1Device> m_d2dDevice;
    winrt::com_ptr<ID2D1DeviceContext> m_d2dContext;
    winrt::com_ptr<IWICImagingFactory2> m_wicFactory;
};