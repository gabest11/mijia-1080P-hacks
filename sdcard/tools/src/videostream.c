// based on https://github.com/labcoder/simple-webserver/blob/master/server.c
// it does not work yet

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "gmlib.h"

#define BUFSIZE 8096
#define ERROR 42
#define SORRY 43
#define LOG   44

#define GET_VIDEOSTREAM_CGI "GET /cgi-bin/videostream.cgi"
#define MAX_SNAPHSOT_LEN (1920 * 1080 * 3 / 2)

int _term_signal = 0;

void term_handler(int sig)
{
	_term_signal = 1;
}

void web_log(int type, char *s1, char *s2, int num)
{
	int fd;
	char logbuffer[BUFSIZE * 2];

	switch(type)
	{
	case ERROR: 
		sprintf(logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2, errno, getpid()); 
		break;
	case SORRY:
		sprintf(logbuffer, "<HTML><BODY><H1>Web Server Sorry: %s %s</H1></BODY></HTML>\r\n", s1, s2);
		write(num, logbuffer, strlen(logbuffer));
		sprintf(logbuffer, "SORRY: %s:%s", s1, s2);
		break;
	case LOG: 
		sprintf(logbuffer, " INFO: %s:%s:%d", s1, s2, num); 
		break;
	}

	if((fd = open("server.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0)
	{
		write(fd, logbuffer, strlen(logbuffer));
		write(fd, "\n", 1);
		close(fd);
	}

	if(type == ERROR || type == SORRY)
	{
		exit(3);
	}
}

int web_snapshot(int fd, int hit, void* bindfd, char* snapshot_buf)
{
	long ret;
	static char buffer[BUFSIZE + 1];
	snapshot_t snapshot;
	int snapshot_len = 0;

	snapshot.bindfd = bindfd;
	snapshot.image_quality = 80;  // The value of image quality from 1(worst) ~ 100(best)
	snapshot.bs_width = 640;
	snapshot.bs_height = 360;
	snapshot.bs_buf = snapshot_buf;
	snapshot.bs_buf_len = MAX_SNAPHSOT_LEN;

	snapshot_len = gm_request_snapshot(&snapshot, 2000); // Timeout value 500ms

	if(snapshot_len < 0) 
	{
		sprintf(buffer, "len=%d,buf=%p", snapshot_len, snapshot_buf);
		web_log(LOG, "GM REQUEST SNAPSHOT", buffer, hit);
		return -1;
	}

	sprintf(buffer, 
		"Content-Type: image/jpeg\r\n"
		"Content-Length: %d\r\n"
		"\r\n", 
		snapshot_len);

	ret = write(fd, buffer, strlen(buffer));

	while(snapshot_len > 0)
	{
		if((ret = write(fd, snapshot_buf, snapshot_len)) < 0) 
		{
			return ret;
		}

		snapshot_buf += ret;
		snapshot_len -= ret;
	}

	return snapshot_len;
}

void web(int fd, int hit)
{
	long i, ret;
	static char buffer[BUFSIZE + 1];

	DECLARE_ATTR(cap_attr, gm_cap_attr_t);
	DECLARE_ATTR(h264e_attr, gm_h264e_attr_t);

	gm_system_t gm_system;
	void* capture_object = NULL;
	void* encode_object = NULL;
	void* groupfd = NULL;
	void* bindfd = NULL;
	char* snapshot_buf = NULL;

	ret = read(fd, buffer, BUFSIZE);

	if(ret == 0 || ret == -1)
	{
		web_log(SORRY, "failed to read browser request", "", fd);
	}

	buffer[ret > 0 && ret < BUFSIZE ? ret : 0] = 0;

	for(i = 0; i < ret; i++)
	{
		if(buffer[i] == '\r' || buffer[i] == '\n')
		{
			buffer[i] = '*';
		}
	}

	web_log(LOG, "request", buffer, hit);

	if(strncmp(buffer, GET_VIDEOSTREAM_CGI, strlen(GET_VIDEOSTREAM_CGI)))
	{
		web_log(SORRY, "Only " GET_VIDEOSTREAM_CGI " is supported", buffer, fd);

		exit(1);
	}

	sprintf(buffer, 
		"HTTP/1.1 200 OK\r\n"
		"Server: Xiaomi Mijia Camera\r\n"
		"Connection: close\r\n"
		"Content-Type: multipart/x-mixed-replace;boundary=ipcamera\r\n"
		"X-Accel-Buffering: no\r\n"
		"\r\n"
		"--ipcamera\r\n");

	write(fd, buffer, strlen(buffer));

	web_log(LOG, "GM INIT", NULL, hit);

	gm_init();
	gm_get_sysinfo(&gm_system);

	groupfd = gm_new_groupfd(); // create new record group fd

	capture_object = gm_new_obj(GM_CAP_OBJECT); // new capture object
	cap_attr.cap_vch = 0;
	//GM8210 capture path 0(liveview), 1(CVBS), 2(can scaling), 3(can scaling)
	//GM8139/GM8287 capture path 0(liveview), 1(CVBS), 2(can scaling), 3(can't scaling down)
	cap_attr.path = 2;
	cap_attr.enable_mv_data = 0;
	gm_set_attr(capture_object, &cap_attr); // set capture attribute

	encode_object = gm_new_obj(GM_ENCODER_OBJECT);  // // create encoder object

	h264e_attr.dim.width = gm_system.cap[cap_attr.cap_vch].dim.width;
	h264e_attr.dim.height = gm_system.cap[cap_attr.cap_vch].dim.height;
	h264e_attr.frame_info.framerate = gm_system.cap[cap_attr.cap_vch].framerate;
	h264e_attr.ratectl.mode = GM_CBR; // GM_CBR, GM_VBR, GM_ECBR, GM_EVBR
	h264e_attr.ratectl.gop = 60;
	h264e_attr.ratectl.bitrate = 2048;  // 2Mbps
	h264e_attr.b_frame_num = 0;  // B-frames per GOP (H.264 high profile)
	h264e_attr.enable_mv_data = 0;  // disable H.264 motion data output
	gm_set_attr(encode_object, &h264e_attr);

	bindfd = gm_bind(groupfd, capture_object, encode_object);

	if(gm_apply(groupfd) >= 0)
	{
		snapshot_buf = (char*)malloc(MAX_SNAPHSOT_LEN);

		if(snapshot_buf != NULL)
		{
			while(_term_signal == 0)
			{
				if((ret = web_snapshot(fd, hit, bindfd, snapshot_buf)) < 0) 
				{
					sprintf(buffer, "%d", (int)ret);

					web_log(LOG, "SNAPSHOT", buffer, hit);

					break;
				}

				sprintf(buffer, "\r\n--ipcamera\r\n");

				ret = write(fd, buffer, strlen(buffer));
			}

			free(snapshot_buf);
		}
		else
		{
			web_log(LOG, "GM INIT", "Not enough memory", hit);
		}
	}
	else
	{
		web_log(LOG, "GM APPLY", NULL, hit);
	}

	gm_unbind(bindfd);
	gm_apply(groupfd);
	gm_delete_obj(encode_object);
	gm_delete_obj(capture_object);
	gm_delete_groupfd(groupfd);
	gm_release();

	sprintf(buffer, "_term_signal=%d", _term_signal);

	web_log(LOG, "DISCONNECT", buffer, hit);

	exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd, hit;
	size_t length;
	static struct sockaddr_in cli_addr;
	static struct sockaddr_in serv_addr;

	if(argc < 2 || argc > 2 || !strcmp(argv[1], "-?"))
	{
		printf("usage: server [port] &\n");

		exit(0);
	}

	if(fork() != 0) return 0;

	signal(SIGCLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, term_handler);
	signal(SIGKILL, term_handler);

	for(i = 0; i < 32; i++)
	{
		close(i);
	}

	setpgrp();

	web_log(LOG, "http server starting", argv[1], getpid());

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		web_log(ERROR, "system call", "socket", 0);
	}

	port = atoi(argv[1]);

	if(port < 0 || port > 60000)
	{
		web_log(ERROR, "Invalid port number try [1,60000]", argv[1], 0);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if(bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		web_log(ERROR, "system call", "bind", 0);
	}

	if(listen(listenfd, 64) < 0)
	{
		web_log(ERROR, "system call", "listen", 0);
	}

	for(hit = 1; _term_signal == 0; hit++)
	{
		length = sizeof(cli_addr);

		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
		{
			web_log(ERROR, "system call", "accept", 0);
		}

		if((pid = fork()) < 0)
		{
			web_log(ERROR, "system call", "fork", 0);
		}
		else
		{
			if(pid == 0)
			{
				close(listenfd);

				web(socketfd, hit);
			}
			else
			{
				close(socketfd);
			}
		}
	}

	exit(0);
}
