#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

using namespace std;

#define __STDC_CONSTANT_MACROS

//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
#ifdef __cplusplus
};
#endif

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

//Buffer:
//|-----------|-------------|
//chunk-------pos---len-----|
static  uint8_t  *audio_chunk; 
static  uint32_t  audio_len; 
static  uint8_t  *audio_pos; 

/* The audio function callback takes the following parameters: 
 * stream: A pointer to the audio buffer to be filled 
 * len: The length (in bytes) of the audio buffer 
*/ 
void  fill_audio(void *udata,uint8_t *stream,int len){ 
	if(audio_len==0)
		return; 

	len=(len>audio_len?audio_len:len);	/*  Mix  as  much  data  as  possible  */ 
	audio_pos += len; 
	audio_len -= len; 
} 
//-----------------


int main(int argc, char* argv[])
{
    av_register_all();
    avcodec_register_all();
    
    string outFileName = "test.aac";
    AVFormatContext *outFormatCtx;
    AVOutputFormat *outFmt;
    outFormatCtx = avformat_alloc_context();
    outFmt = av_guess_format(nullptr, outFileName.c_str(), nullptr);
    outFormatCtx->oformat = outFmt;

    if(avio_open(&outFormatCtx->pb, outFileName.c_str(), AVIO_FLAG_READ_WRITE) < 0)
    {
        printf("open out fmt error\n");
        return -1;
    }

    AVStream* audioSt = avformat_new_stream(outFormatCtx, 0);
    if(audioSt == nullptr)
    {
        printf("avformat_new_stream error\n");
        return -1;
    }

    av_dump_format(outFormatCtx, 0, outFileName.c_str(), 1);

    avformat_write_header(outFormatCtx, nullptr);

	AVFormatContext	*pFormatCtx;
	int				i, audioStream;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVPacket		*packet;
	uint8_t			*out_buffer;
	AVFrame			*pFrame;
    
    // init encodeCtx
    string encoderName = "libfdk_aac";
    AVCodec* avcodec = avcodec_find_encoder_by_name("libfdk_aac");
    if(avcodec == nullptr)
    {
        printf("avcodec_find_encoder_by_name error\n");
        return -1;
    }

    AVCodecContext  *encodeCtx;
    encodeCtx = avcodec_alloc_context3(avcodec);
    if(encodeCtx == nullptr)
    {
        printf("avcodec_alloc_context3 error\n");
        return -1;
    }
    
    encodeCtx->codec_id = AV_CODEC_ID_AAC;
    encodeCtx->codec_type = AVMEDIA_TYPE_AUDIO;

    encodeCtx->bit_rate = 192000;
    encodeCtx->sample_fmt = AV_SAMPLE_FMT_S16;
    encodeCtx->sample_rate = 44100;
    encodeCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    encodeCtx->channels = 2;

    int ret = avcodec_open2(encodeCtx, encodeCtx->codec, nullptr);
    if(ret < 0)
    {
        printf("avcodec_open2 error");
        return -1;
    }

    AVAudioFifo* fifo = nullptr;

	uint32_t len = 0;
	int got_picture;
	int index = 0;
	int64_t in_channel_layout;
	struct SwrContext *au_convert_ctx;

	FILE *pFile=NULL;
	char url[]="xiaoqingge.mp3";

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	//Open
	if(avformat_open_input(&pFormatCtx,url,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	// Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, url, false);

	// Find the first audio stream
	audioStream=-1;
	for(i=0; i < pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			audioStream=i;
			break;
		}

	if(audioStream==-1){
		printf("Didn't find a audio stream.\n");
		return -1;
	}

	// Get a pointer to the codec context for the audio stream
	pCodecCtx=pFormatCtx->streams[audioStream]->codec;

	// Find the decoder for the audio stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL){
		printf("Codec not found.\n");
		return -1;
	}

	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
		printf("Could not open codec.\n");
		return -1;
	}

	
	packet=(AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	//Out Audio Param
	uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples=pCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
	int out_sample_rate=44100;
	int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);

	out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
	pFrame=av_frame_alloc();

	//FIX:Some Codec's Context Information is missing
	in_channel_layout=av_get_default_channel_layout(pCodecCtx->channels);
	//Swr

	au_convert_ctx = swr_alloc();
	au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);
	swr_init(au_convert_ctx);

	while(av_read_frame(pFormatCtx, packet)>=0){
		if(packet->stream_index==audioStream){
			ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, packet);
			if ( ret < 0 ) {
                printf("Error in decoding audio frame.\n");
                return -1;
            }
			if ( got_picture > 0 ){
				int resample_size = swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);
                if(resample_size < 0)
                {
                    return resample_size;
                }
		        printf("========= index:%5d\t pts:%lld\t packet size:%d\n",index,packet->pts,packet->size);
                

                if(fifo == nullptr)
                {
                    fifo = av_audio_fifo_alloc(out_sample_fmt, out_channels, out_sample_rate);
                }

                if(fifo != nullptr)
                {
                    int size = av_samples_get_buffer_size(nullptr, out_channels, resample_size, out_sample_fmt, 0);
                    printf("resample_size:%d outbuf size:%d size:%d\n", resample_size, out_buffer_size, size);
                    av_audio_fifo_write(fifo, (void**)&out_buffer, resample_size);
                    printf("fifo size:%d\n", av_audio_fifo_size(fifo));
                }

                while(av_audio_fifo_size(fifo) > 1024)
                {
                    AVFrame* avframe = av_frame_alloc();
                    if(avframe == nullptr)
                    {
                        return -1;
                    }
                    avframe->nb_samples = 1024;
                    avframe->channels = 2;
                    avframe->channel_layout = av_get_default_channel_layout(2);
                    avframe->format = AV_SAMPLE_FMT_S16;
                    avframe->sample_rate = out_sample_rate;
                    avframe->pkt_dts = (1000 / (44100 / 1024)) * index;
                    avframe->pkt_pts = avframe->pkt_dts;
                    avframe->pts = avframe->pkt_dts;
                    int error = av_frame_get_buffer(avframe, 0);
                    if(error < 0)
                    {
                        printf("av_frame_get_buffer error\n");
                        return -1;
                    }
                    int read_size = av_audio_fifo_read(fifo, (void**)avframe->data, 1024);
                    printf("read size:%d\n", read_size);
                    printf("after read size:%d\n", av_audio_fifo_size(fifo));
                    //av_frame_free(&avframe);
                    AVPacket avpacket;
                    int encode_success = 0;
                    int size = av_samples_get_buffer_size(NULL, encodeCtx->channels, encodeCtx->frame_size, encodeCtx->sample_fmt, 1);
                    printf("packet size:%d\n", size);
                    av_new_packet(&avpacket, size);
                    int ret = avcodec_encode_audio2(encodeCtx, &avpacket, avframe, &encode_success);
                    if(ret >=0 && encode_success == 1)
                    {
                        av_write_frame(outFormatCtx, &avpacket);
                        printf("encoder success frame dts:%d  pkt dts:%d\n", avframe->pkt_dts, avpacket.dts);
                    }
				    index++;
                    printf("index:%d\n", index);
                }

			}

		}
		av_free_packet(packet);
	}

    av_write_trailer(outFormatCtx);

	swr_free(&au_convert_ctx);

	
	av_free(out_buffer);
	avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

	return 0;
}


