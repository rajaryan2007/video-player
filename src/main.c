#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

uint64_t audio_bytes_written = 0;
double audio_clock = 0.0;



int main(void) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window* window;
    SDL_Renderer* renderer;
    //SDL_CreateWindowAndRenderer("Codotaku video player", 800, 600, SDL_WINDOW_RESIZABLE, &window, &renderer);
    window = SDL_CreateWindow("video player", 800, 600, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateGPURenderer(NULL, window);
    SDL_GPUDevice* gpu = SDL_GetGPURendererDevice(renderer);
    SDL_MaximizeWindow(window);



    AVFormatContext* format_context = NULL;
    avformat_open_input(&format_context, "C:\\Users\\rion\\Videos\\s4.mp4",
        NULL, NULL
    );

    const AVCodec* codec = NULL;
    int video_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    AVStream* video_stream = format_context->streams[video_stream_index];

    AVCodecContext* decoder = avcodec_alloc_context3(codec);
    decoder->thread_count = 0;
    avcodec_parameters_to_context(decoder, format_context->streams[video_stream_index]->codecpar);

    avcodec_open2(decoder, codec, NULL);

    int a_idx = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    AVCodecContext* a_dec = avcodec_alloc_context3(avcodec_find_decoder(format_context->streams[a_idx]->codecpar->codec_id));
    avcodec_parameters_to_context(a_dec, format_context->streams[a_idx]->codecpar);
    avcodec_open2(a_dec, NULL, NULL);
    SwrContext* swr = swr_alloc();
    swr_alloc_set_opts2(&swr, &a_dec->ch_layout, AV_SAMPLE_FMT_FLT, a_dec->sample_rate,
        &a_dec->ch_layout, a_dec->sample_fmt, a_dec->sample_rate, 0, NULL);
    swr_init(swr);
    AVPacket packet;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice((SDL_AudioDeviceID)0xFFFFFFFFu, NULL);
    SDL_AudioSpec spec = { SDL_AUDIO_F32, a_dec->ch_layout.nb_channels, a_dec->sample_rate };
    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &spec); // Same spec because Swr handles conversion
    SDL_BindAudioStream(dev, stream);
    SDL_ResumeAudioDevice(dev);

    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    avformat_find_stream_info(format_context, NULL);

    const int audio_bytes_per_sample = sizeof(float); // SDL_AUDIO_F32
    const int audio_channels = a_dec->ch_layout.nb_channels;
    const int audio_sample_rate = a_dec->sample_rate;

    const int audio_bytes_per_second =
        audio_sample_rate * audio_channels * audio_bytes_per_sample;


    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        decoder->width,
        decoder->height
    );

    Uint64 start_ns = 0;


    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT)
                return 0;

        if (av_read_frame(format_context, &packet) >= 0) {
            if (packet.stream_index == a_idx) {
                avcodec_send_packet(a_dec, &packet);
                while (avcodec_receive_frame(a_dec, frame) == 0) {
                   
                    uint8_t* output;
                    int out_samples = av_rescale_rnd(swr_get_delay(swr, a_dec->sample_rate) + frame->nb_samples, a_dec->sample_rate, a_dec->sample_rate, AV_ROUND_UP);
                    av_samples_alloc(&output, NULL, a_dec->ch_layout.nb_channels, out_samples, AV_SAMPLE_FMT_FLT, 0);
                    int count = swr_convert(swr, &output, out_samples, (const uint8_t**)frame->data, frame->nb_samples);
                    
                    // Standard SDL push (Packed data)
                    

                    int data_size = count * a_dec->ch_layout.nb_channels * sizeof(float);
                    SDL_PutAudioStreamData(stream, output, data_size);

                    audio_bytes_written += data_size;

                   


                    av_freep(&output);
                }
            }
            else if (packet.stream_index == video_stream_index) {
                
                avcodec_send_packet(decoder, &packet);
                while (avcodec_receive_frame(decoder, frame) == 0) {
                  uint64_t queued_bytes = SDL_GetAudioStreamQueued(stream);
                  int64_t played_bytes =
                      (int64_t)audio_bytes_written - (int64_t)queued_bytes;

                  if (played_bytes < 0)
                      played_bytes = 0;

                  double master_audio_clock =
                      (double)played_bytes / (double)audio_bytes_per_second;
                  int64_t pts = frame->best_effort_timestamp;
                  if (pts == AV_NOPTS_VALUE)
                      continue;

                  double video_pts = pts * av_q2d(video_stream->time_base);

                    double diff = video_pts - master_audio_clock;

                    if (diff > 0.01) {
                        if (diff > 0.1) diff = 0.1;
                        SDL_Delay((Uint32)(diff * 1000));
                    }
                    else if (diff < -0.05) {
                        continue; // DROP late frame
                    }


                    SDL_UpdateYUVTexture(texture, NULL,
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]);

                    // 3. Render Aspect Ratio Corrected Frame
                    int w, h;
                    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
                    float scale = SDL_min((float)w / decoder->width, (float)h / decoder->height);
                    SDL_FRect dstrect = {
                        ((float)w - (decoder->width * scale)) / 2,
                        ((float)h - (decoder->height * scale)) / 2,
                        decoder->width * scale,
                        decoder->height * scale
                    };

                    SDL_RenderClear(renderer);
                    SDL_RenderTexture(renderer, texture, NULL, &dstrect);
                    SDL_RenderPresent(renderer);
                }
            }
            av_packet_unref(&packet);
        }
    }
}