#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>


int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window;
    SDL_Renderer* renderer;
    //SDL_CreateWindowAndRenderer("Codotaku video player", 800, 600, SDL_WINDOW_RESIZABLE, &window, &renderer);
    window = SDL_CreateWindow("video player", 800, 600, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateGPURenderer(NULL, window);
    SDL_GPUDevice* gpu = SDL_GetGPURendererDevice(renderer);
    SDL_MaximizeWindow(window);
    
   

    AVFormatContext* format_context = NULL;
    avformat_open_input(&format_context, "C:\\Users\\rion\\Videos\\itachi.mp4",
        NULL, NULL
    );

    const AVCodec* codec = NULL;
    int video_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    AVStream* video_stream = format_context->streams[video_stream_index];

    AVCodecContext* decoder = avcodec_alloc_context3(codec);
    decoder->thread_count = 0;
    avcodec_parameters_to_context(decoder, format_context->streams[video_stream_index]->codecpar);

    avcodec_open2(decoder, codec, NULL);

    AVPacket packet;
    AVFrame* frame = av_frame_alloc();

    avformat_find_stream_info(format_context, NULL);

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
            if (packet.stream_index == video_stream_index) {
                avcodec_send_packet(decoder, &packet);
                while(avcodec_receive_frame(decoder, frame) == 0) {

                    const double frame_time_s = frame->pts * av_q2d(video_stream->time_base);
                    if (start_ns == 0) start_ns = SDL_GetTicksNS();
                    const Uint64 elapsed_time_ns = SDL_GetTicksNS() - start_ns;
                    const double elapsed_time_s = (double)elapsed_time_ns / SDL_NS_PER_SECOND;
                    SDL_Log("%f",elapsed_time_s );

                    const double delay_s = frame_time_s - elapsed_time_s;
                    if (delay_s > 0.001) {
                       
                        SDL_Delay((Uint32)(delay_s * 1000));
                    }
                    else if (delay_s < -0.1) {
                       
                        continue;
                    }

                    SDL_UpdateYUVTexture(
                        texture,
                        NULL,
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]
                    );
                 
                    int w, h;
                    SDL_GetCurrentRenderOutputSize(renderer,&w,&h);
                    float frame_width = (float)decoder->width;
                    float frame_height = (float)decoder->height;
                    
                    float scale_w =(float) w/ frame_width;
                    float scale_h = (float)h / frame_height;

                    float scale = SDL_min(scale_w, scale_h);
                    SDL_FRect dstrect;

                    dstrect.w = frame_width * scale;
                    dstrect.h = frame_height * scale;

                    dstrect.x = ((float)w-dstrect.w) / 2;
                    dstrect.y = ((float)h - dstrect.h) / 2;

                    SDL_RenderTexture(renderer, texture, NULL, &dstrect);
                    SDL_RenderPresent(renderer);
                }
            }
            av_packet_unref(&packet);
        }

        SDL_RenderClear(renderer);
       
    }

}