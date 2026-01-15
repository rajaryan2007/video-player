#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <stdbool.h>

typedef struct PacketQueueNode {
    AVPacket* pkt;
    struct PacketQueueNode* next;
} PacketQueueNode;

typedef struct {
    PacketQueueNode* first, * last;
    int count;
} PacketQueue;

void enqueue(PacketQueue* q, AVPacket* pkt) {
    PacketQueueNode* node = malloc(sizeof(PacketQueueNode));
    node->pkt = av_packet_alloc();
    av_packet_move_ref(node->pkt, pkt);
    node->next = NULL;
    if (!q->last) q->first = node;
    else q->last->next = node;
    q->last = node;
    q->count++;
}

AVPacket* dequeue(PacketQueue* q) {
    if (!q->first) return NULL;
    PacketQueueNode* node = q->first;
    AVPacket* pkt = node->pkt;
    q->first = node->next;
    if (!q->first) q->last = NULL;
    free(node);
    q->count--;
    return pkt;
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    AVFormatContext* fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, "C:\\Users\\rion\\Videos\\s3.mp4", NULL, NULL) < 0) return -1;
    avformat_find_stream_info(fmt_ctx, NULL);

    
    const AVCodec* v_codec = NULL;
    int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &v_codec, 0);
    AVCodecContext* v_dec = avcodec_alloc_context3(v_codec);
    avcodec_parameters_to_context(v_dec, fmt_ctx->streams[v_idx]->codecpar);
    v_dec->thread_count = 0;
    avcodec_open2(v_dec, v_codec, NULL);


    int a_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    AVCodecContext* a_dec = avcodec_alloc_context3(avcodec_find_decoder(fmt_ctx->streams[a_idx]->codecpar->codec_id));
    avcodec_parameters_to_context(a_dec, fmt_ctx->streams[a_idx]->codecpar);
    avcodec_open2(a_dec, NULL, NULL);

    
    SwrContext* swr = swr_alloc();
    swr_alloc_set_opts2(&swr, &a_dec->ch_layout, AV_SAMPLE_FMT_FLT, a_dec->sample_rate,
        &a_dec->ch_layout, a_dec->sample_fmt, a_dec->sample_rate, 0, NULL);
    swr_init(swr);

    SDL_AudioSpec spec = { SDL_AUDIO_F32, a_dec->ch_layout.nb_channels, a_dec->sample_rate };
    SDL_AudioStream* a_stream = SDL_CreateAudioStream(&spec, &spec);
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice((SDL_AudioDeviceID)0xFFFFFFFFu, NULL);
    SDL_BindAudioStream(dev, a_stream);
    SDL_ResumeAudioDevice(dev);

    SDL_Window* win = SDL_CreateWindow("Video Player", 1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* rnd = SDL_CreateGPURenderer(win, NULL);
    SDL_GPUDevice* gpu = SDL_GetGPURendererDevice(rnd);
    SDL_Texture* tex = SDL_CreateTexture(rnd, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, v_dec->width, v_dec->height);

    PacketQueue audio_q = { 0 }, video_q = { 0 };
    AVFrame* frame = av_frame_alloc();
    AVPacket pkt;
    uint64_t audio_bytes_sent = 0;
    bool quit = false;

    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) if (ev.type == SDL_EVENT_QUIT) quit = true;

 
        if (audio_q.count < 100 && video_q.count < 100) {
            if (av_read_frame(fmt_ctx, &pkt) >= 0) {
                if (pkt.stream_index == a_idx) enqueue(&audio_q, &pkt);
                else if (pkt.stream_index == v_idx) enqueue(&video_q, &pkt);
                else av_packet_unref(&pkt);
            }
        }

      
        if (audio_q.count > 0 && SDL_GetAudioStreamQueued(a_stream) < (spec.freq * spec.channels * 4)) {
            AVPacket* p = dequeue(&audio_q);
            if (avcodec_send_packet(a_dec, p) == 0) {
                while (avcodec_receive_frame(a_dec, frame) == 0) {
                    uint8_t* out;
                    int out_samples = av_rescale_rnd(swr_get_delay(swr, a_dec->sample_rate) + frame->nb_samples, a_dec->sample_rate, a_dec->sample_rate, AV_ROUND_UP);
                    av_samples_alloc(&out, NULL, a_dec->ch_layout.nb_channels, out_samples, AV_SAMPLE_FMT_FLT, 0);
                    int converted = swr_convert(swr, &out, out_samples, (const uint8_t**)frame->data, frame->nb_samples);
                    int size = converted * a_dec->ch_layout.nb_channels * sizeof(float);
                    SDL_PutAudioStreamData(a_stream, out, size);
                    audio_bytes_sent += size;
                    av_freep(&out);
                }
            }
            av_packet_free(&p);
        }


        if (video_q.count > 0) {
            
            double audio_clock = (double)(audio_bytes_sent - SDL_GetAudioStreamQueued(a_stream)) / (a_dec->sample_rate * a_dec->ch_layout.nb_channels * 4);

            AVPacket* p = video_q.first->pkt;
            double v_pts = p->pts * av_q2d(fmt_ctx->streams[v_idx]->time_base);

            // sync logic
            if (v_pts <= audio_clock) {
                p = dequeue(&video_q);
                if (avcodec_send_packet(v_dec, p) == 0) {
                    if (avcodec_receive_frame(v_dec, frame) == 0) {
                        SDL_UpdateYUVTexture(tex, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);

                      
                        int w, h;
                        SDL_GetRenderOutputSize(rnd, &w, &h);
                        float scale = SDL_min((float)w / v_dec->width, (float)h / v_dec->height);
                        SDL_FRect dstrect = { ((float)w - (v_dec->width * scale)) / 2, ((float)h - (v_dec->height * scale)) / 2, v_dec->width * scale, v_dec->height * scale };

                        SDL_RenderClear(rnd);
                        SDL_RenderTexture(rnd, tex, NULL, &dstrect);
                        SDL_RenderPresent(rnd);
                    }
                }
                av_packet_free(&p);
            }
        }
        SDL_Delay(1);
    }

    avformat_close_input(&fmt_ctx);
    return 0;
}