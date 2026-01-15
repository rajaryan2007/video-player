#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);



    AVFormatContext* fmt_ctx = NULL;
    avformat_open_input(&fmt_ctx, "C:\\Users\\rion\\Videos\\s4.mp4", NULL, NULL);
    avformat_find_stream_info(fmt_ctx, NULL);

    const AVCodec* v_codec = avcodec_find_decoder(fmt_ctx->streams[av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)]->codecpar->codec_id);
    AVCodecContext* v_dec = avcodec_alloc_context3(v_codec);
    avcodec_parameters_to_context(v_dec, fmt_ctx->streams[av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)]->codecpar);
    avcodec_open2(v_dec, v_codec, NULL);

    int a_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    AVCodecContext* a_dec = avcodec_alloc_context3(avcodec_find_decoder(fmt_ctx->streams[a_idx]->codecpar->codec_id));
    avcodec_parameters_to_context(a_dec, fmt_ctx->streams[a_idx]->codecpar);
    avcodec_open2(a_dec, NULL, NULL);


    SwrContext* swr = swr_alloc();
    swr_alloc_set_opts2(&swr, &a_dec->ch_layout, AV_SAMPLE_FMT_FLT, a_dec->sample_rate,
        &a_dec->ch_layout, a_dec->sample_fmt, a_dec->sample_rate, 0, NULL);
    swr_init(swr);

    // --- SDL AUDIO ---
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice((SDL_AudioDeviceID)0xFFFFFFFFu, NULL);
    SDL_AudioSpec spec = { SDL_AUDIO_F32, a_dec->ch_layout.nb_channels, a_dec->sample_rate };
    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &spec); // Same spec because Swr handles conversion
    SDL_BindAudioStream(dev, stream);
    SDL_ResumeAudioDevice(dev);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    SDL_Window* win = SDL_CreateWindow("Fixed Audio", 800, 600, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, NULL);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, v_dec->width, v_dec->height);

    Uint64 start = SDL_GetTicksNS();

    while (true) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) if (ev.type == SDL_EVENT_QUIT) return 0;

        if (av_read_frame(fmt_ctx, pkt) >= 0) {
            if (pkt->stream_index == a_idx) {
                avcodec_send_packet(a_dec, pkt);
                while (avcodec_receive_frame(a_dec, frame) == 0) {
                    // Convert Planar to Packed
                    uint8_t* output;
                    int out_samples = av_rescale_rnd(swr_get_delay(swr, a_dec->sample_rate) + frame->nb_samples, a_dec->sample_rate, a_dec->sample_rate, AV_ROUND_UP);
                    av_samples_alloc(&output, NULL, a_dec->ch_layout.nb_channels, out_samples, AV_SAMPLE_FMT_FLT, 0);
                    int count = swr_convert(swr, &output, out_samples, (const uint8_t**)frame->data, frame->nb_samples);

                    // Standard SDL push (Packed data)
                    SDL_PutAudioStreamData(stream, output, count * a_dec->ch_layout.nb_channels * sizeof(float));
                    av_freep(&output);
                }
            }
            else {
                avcodec_send_packet(v_dec, pkt);
                if (avcodec_receive_frame(v_dec, frame) == 0) {
                    SDL_UpdateYUVTexture(tex, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);
                    SDL_RenderClear(ren);
                    SDL_RenderTexture(ren, tex, NULL, NULL);
                    SDL_RenderPresent(ren);
                    // Minimal sync
                    SDL_Delay(25);
                }
            }
            av_packet_unref(pkt);
        }
    }
    return 0;
}