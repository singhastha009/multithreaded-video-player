#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <unistd.h>

#define BUFFER_SIZE 10 // Circular buffer size

// Frame buffer structure
typedef struct {
    uint8_t *data;
    int width;
    int height;
} FrameBuffer;

FrameBuffer frame_buffer[BUFFER_SIZE];
int write_index = 0, read_index = 0;
int buffer_count = 0;
pthread_mutex_t lock;
pthread_cond_t cond;

AVFormatContext *fmt_ctx;
AVCodecContext *codec_ctx;
struct SwsContext *sws_ctx;
int video_stream_index;
GtkWidget *image;

void *decode_thread(void *arg) {
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            avcodec_send_packet(codec_ctx, &packet);
            if (avcodec_receive_frame(codec_ctx, frame) == 0) {
                pthread_mutex_lock(&lock);
                while (buffer_count == BUFFER_SIZE) {
                    pthread_cond_wait(&cond, &lock);
                }
                
                int rgb_size = codec_ctx->width * codec_ctx->height * 3;
                frame_buffer[write_index].data = (uint8_t *)malloc(rgb_size);
                frame_buffer[write_index].width = codec_ctx->width;
                frame_buffer[write_index].height = 480;
                
                uint8_t *rgb_data[1] = {frame_buffer[write_index].data};
                int rgb_linesize[1] = {3 * codec_ctx->width};
                sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0,
                          codec_ctx->height, rgb_data, rgb_linesize);
                
                write_index = (write_index + 1) % BUFFER_SIZE;
                buffer_count++;
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&lock);
            }
        }
        av_packet_unref(&packet);
    }
    av_frame_free(&frame);
    return NULL;
}

static void render_frame(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    pthread_mutex_lock(&lock);
    if (buffer_count > 0) {
        FrameBuffer *frame = &frame_buffer[read_index];
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(frame->data, GDK_COLORSPACE_RGB,
                                                     FALSE, 8, frame->width, frame->height,
                                                     frame->width * 3, NULL, NULL);
        gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        cairo_paint(cr);
        g_object_unref(pixbuf);
        read_index = (read_index + 1) % BUFFER_SIZE;
        buffer_count--;
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&lock);
    
}

static gboolean refresh_frame(gpointer data) {
    if (!GTK_IS_WIDGET(data)) return G_SOURCE_CONTINUE;
    gtk_widget_queue_draw(GTK_WIDGET(data));
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Video Player");
    gtk_window_set_default_size(GTK_WINDOW(window), codec_ctx->width * 480 / codec_ctx->height, 480);
    
    image = gtk_drawing_area_new();
    gtk_window_set_child(GTK_WINDOW(window), image);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(image), render_frame, NULL, NULL);
    
    g_timeout_add((int)(1000.0 / *(double *)user_data), refresh_frame, image);
    
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input video> <frame rate>\n", argv[0]);
        exit(1);
    }

    double frame_rate = atof(argv[2]);
    avformat_network_init();
    fmt_ctx = avformat_alloc_context();
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) {
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        return -1;
    }
    avformat_find_stream_info(fmt_ctx, NULL);

    video_stream_index = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        fprintf(stderr, "No video stream found in %s\n", argv[1]);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
    const AVCodec *codec = avcodec_find_decoder(codec_ctx->codec_id);
    avcodec_open2(codec_ctx, codec, NULL);

    int scaled_width = codec_ctx->width * 480 / codec_ctx->height;
sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                         scaled_width, 480, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
    
    pthread_t decodeThread;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_create(&decodeThread, NULL, decode_thread, NULL);
    
    GtkApplication *app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &frame_rate);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
    
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(sws_ctx);
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    
    return status;
}