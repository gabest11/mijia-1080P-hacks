#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include "gmlib.h"

// FIXME: /gm/config/gmlib.cfg limits max snapshot resolution to 640x360

#define MAX_SNAPHSOT_LEN (1920 * 1080 * 3 / 2)

gm_system_t gm_system;
void* capture_object = NULL;
void* encode_object = NULL;
void* groupfd = NULL;
void* bindfd = NULL;

int main(int argc, char* argv[])
{
	DECLARE_ATTR(cap_attr, gm_cap_attr_t);
	DECLARE_ATTR(h264e_attr, gm_h264e_attr_t);

	int ch = 0;

	FILE* fp = NULL;

	snapshot_t snapshot;
	char* snapshot_buf = NULL;
	int snapshot_len = 0;

	int quality = 80;
	int width = 640;
	int height = 360;

	if(argc >= 3)
	{
		int q = atoi(argv[2]);

		if(q > 0 && q <= 100) quality = q;
	}

	if(argc >= 4)
	{
		int w = atoi(argv[3]);

		if(w >= 176 && w <= 1920)
		{
			width = w;
			height = w * 9 / 16;
		}
	}

	if(argc >= 5)
	{
		int h = atoi(argv[4]);

		if(h >= 144 && h <= 1080)
		{
			height = h;
		}
	}

	//

	gm_init();
	gm_get_sysinfo(&gm_system);

	groupfd = gm_new_groupfd(); // create new record group fd

	capture_object = gm_new_obj(GM_CAP_OBJECT); // new capture object
	cap_attr.cap_vch = ch;
	//GM8210 capture path 0(liveview), 1(CVBS), 2(can scaling), 3(can scaling)
	//GM8139/GM8287 capture path 0(liveview), 1(CVBS), 2(can scaling), 3(can't scaling down)
	cap_attr.path = 2;
	cap_attr.enable_mv_data = 0;
	gm_set_attr(capture_object, &cap_attr); // set capture attribute

	encode_object = gm_new_obj(GM_ENCODER_OBJECT);  // // create encoder object

	h264e_attr.dim.width = gm_system.cap[ch].dim.width;
	h264e_attr.dim.height = gm_system.cap[ch].dim.height;
	h264e_attr.frame_info.framerate = gm_system.cap[ch].framerate;
	h264e_attr.ratectl.mode = GM_CBR; // GM_CBR, GM_VBR, GM_ECBR, GM_EVBR
	h264e_attr.ratectl.gop = 60;
	h264e_attr.ratectl.bitrate = 2048;  // 2Mbps
	h264e_attr.b_frame_num = 0;  // B-frames per GOP (H.264 high profile)
	h264e_attr.enable_mv_data = 0;  // disable H.264 motion data output
	gm_set_attr(encode_object, &h264e_attr);

	bindfd = gm_bind(groupfd, capture_object, encode_object);

	if(gm_apply(groupfd) < 0)
	{
        	perror("gm_apply fail");
	        return -1;
	}
/*
	if(pthread_create(&enc_thread_id, NULL, encode_thread, (void *)ch) < 0)
	{
	        perror("Create encode thread failed");
	        return -1;
	}
*/
	//

	snapshot_buf = (char*)malloc(MAX_SNAPHSOT_LEN);
	
	if(snapshot_buf == NULL)
	{
        	perror("Not enough memory");
		return -1;
	}

	snapshot.bindfd = bindfd;
	snapshot.image_quality = quality;  // The value of image quality from 1(worst) ~ 100(best)
	snapshot.bs_buf = snapshot_buf;
	snapshot.bs_buf_len = MAX_SNAPHSOT_LEN;
	snapshot.bs_width = width;
	snapshot.bs_height = height;

	snapshot_len = gm_request_snapshot(&snapshot, 500); // Timeout value 500ms

	if(snapshot_len >= 0)
	{
		fp = fopen(argv[1], "wb");

		if(fp == NULL)
		{
			perror("cannot open output file");
			snapshot_len = -1;
		}
		else
		{
			fwrite(snapshot_buf, 1, snapshot_len, fp);
			fclose(fp);
		}
	}
	else
	{
        	perror("gm_request_snapshot failed");
	}

	gm_unbind(bindfd);
	gm_apply(groupfd);
	gm_delete_obj(encode_object);
	gm_delete_obj(capture_object);
	gm_delete_groupfd(groupfd);
	gm_release();

	free(snapshot_buf);

	return snapshot_len;
}
