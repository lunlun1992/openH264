#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "libavutil/avutil.h"
#include <libavcodec/avcodec.h>

int main(int argc, char **argv)
{
	AVCodecContext *pCodecCtx = NULL;
	AVCodecParserContext *pParserCtx = NULL;
	AVCodec *pCodec = NULL;
	AVPacket packet = { 0 };
	AVFrame *pFrame = NULL;
	int ret = 0;
	FILE *filein = NULL;
	FILE *fileout = NULL;
	int inputlength = 0;
	uint8_t *inputbytes = NULL;
	uint8_t *outputyuv = NULL;
	uint8_t *pdata = NULL;
	int lendata = 0;
	int i, j;

	filein = fopen("one.h264", "rb");
	if (!filein)
	{
		av_log(NULL, AV_LOG_ERROR, "open input file error\n");
		goto fail;
	}
	fseek(filein, 0, SEEK_END);
	inputlength = ftell(filein);
	inputbytes = (uint8_t *)malloc(inputlength);
	if (!inputbytes)
	{
		av_log(NULL, AV_LOG_ERROR, "malloc fail\n");
		goto fail;
	}
	fseek(filein, 0, SEEK_SET);
	if (inputlength != fread(inputbytes, 1, inputlength, filein))
	{
		av_log(NULL, AV_LOG_ERROR, "read file error\n");
		goto fail;
	}
	fclose(filein);
	filein = NULL;

	fileout = fopen("out.yuv", "wb");

	avcodec_register_all();

	pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!pCodec)
	{
		av_log(NULL, AV_LOG_ERROR, "find decoder error\n");
		goto fail;
	}

	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (!pCodecCtx)
	{
		av_log(NULL, AV_LOG_ERROR, "alloc decoder context error\n");
		goto fail;
	}

	ret = avcodec_open2(pCodecCtx, pCodec, NULL);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "codec open fail\n");
		goto fail;
	}

	pParserCtx = av_parser_init(AV_CODEC_ID_H264);
	if (!pParserCtx)
	{
		av_log(NULL, AV_LOG_ERROR, "parser open fail\n");
		goto fail;
	}

	pFrame = av_frame_alloc();
	av_init_packet(&packet);

	pdata = inputbytes;
	lendata = inputlength;
	while(lendata > 0)
	{
		ret = av_parser_parse2(pParserCtx, pCodecCtx, &packet.data, &packet.size, pdata, lendata, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		if(ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "parser parse fail\n");
			goto fail;
		}

		pdata += ret;
		lendata -= ret;
		
		if(packet.size)
		{
			ret = avcodec_send_packet(pCodecCtx, &packet);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "send packet fail\n");
				goto fail;
			}
			while (ret >= 0)
			{
				ret = avcodec_receive_frame(pCodecCtx, pFrame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				else if (ret < 0)
				{
					av_log(NULL, AV_LOG_ERROR, "receive frame fail\n");
					goto fail;
				}
				av_log(NULL, AV_LOG_INFO, "receive %d frame, width: %d, height: %d\n", pCodecCtx->frame_number, pFrame->width, pFrame->height);
				for (i = 0; i < pFrame->height; i++)
					fwrite(pFrame->data[0] + i * pFrame->linesize[0], pFrame->width, 1, fileout);
				for (i = 0; i < (pFrame->height >> 1); i++)
					fwrite(pFrame->data[1] + i * pFrame->linesize[1], pFrame->width >> 1, 1, fileout);
				for (i = 0; i < (pFrame->height >> 1); i++)
					fwrite(pFrame->data[2] + i * pFrame->linesize[2], pFrame->width >> 1, 1, fileout);
			}
		}
	}

	ret = avcodec_send_packet(pCodecCtx, NULL);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "send packet fail\n");
		goto fail;
	}
	while (ret >= 0)
	{
		ret = avcodec_receive_frame(pCodecCtx, pFrame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "receive frame fail\n");
			goto fail;
		}
		av_log(NULL, AV_LOG_INFO, "receive %d frame, width: %d, height: %d\n", pCodecCtx->frame_number, pFrame->width, pFrame->height);
		for (i = 0; i < pFrame->height; i++)
			fwrite(pFrame->data[0] + i * pFrame->linesize[0], pFrame->width, 1, fileout);
		for (i = 0; i < (pFrame->height >> 1); i++)
			fwrite(pFrame->data[1] + i * pFrame->linesize[1], pFrame->width >> 1, 1, fileout);
		for (i = 0; i < (pFrame->height >> 1); i++)
			fwrite(pFrame->data[2] + i * pFrame->linesize[2], pFrame->width >> 1, 1, fileout);
	}


	avcodec_free_context(&pCodecCtx);
	fclose(fileout);
	free(inputbytes);
	av_frame_free(&pFrame);
	av_packet_unref(&packet);
	av_parser_close(pParserCtx);
	return 0;
fail:
	if (pCodecCtx)
		avcodec_free_context(&pCodecCtx);
	if (filein)
		fclose(filein);
	if (inputbytes)
		free(inputbytes);
	av_frame_free(&pFrame);
	av_packet_unref(&packet);
	av_parser_close(pParserCtx);
	return -1;
}
