// g++ -o hello_video main.cpp -L/usr/lib/aml_libs/ -lamcodec -lamadec -lamavutils -lasound

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

// The headers are not aware C++ exists
extern "C"
{
	#include <amcodec/codec.h>
}


// Codec parameter flags
//    size_t is used to make it 
//    64bit safe for use on Odroid C2
const size_t EXTERNAL_PTS = 0x01;
const size_t SYNC_OUTSIDE = 0x02;
const size_t USE_IDR_FRAMERATE = 0x04;
const size_t UCODE_IP_ONLY_PARAM = 0x08;
const size_t MAX_REFER_BUF = 0x10;
const size_t ERROR_RECOVERY_MODE_IN = 0x20;

// Buffer size
const int BUFFER_SIZE = 4096;	// 4K (page)


// Global variable(s)
bool isRunning;


// Helper function to enable/disable a framebuffer
int osd_blank(const char *path, int cmd)
{
	int fd;
	char  bcmd[16];
	fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);

	if (fd >= 0) {
		sprintf(bcmd, "%d", cmd);
		if (write(fd, bcmd, strlen(bcmd)) < 0) {
			printf("osd_blank error during write.\n");
		}
		close(fd);
		return 0;
	}

	return -1;
}


// Disable framebuffers
void init_display()
{
	osd_blank("/sys/class/graphics/fb0/blank", 1);
	osd_blank("/sys/class/graphics/fb1/blank", 1);
}


// Enable framebuffers
void restore_display()
{
	osd_blank("/sys/class/graphics/fb0/blank", 0);
	osd_blank("/sys/class/graphics/fb1/blank", 0);
}


// Signal handler
void my_handler(int s)
{	
	isRunning = false;
}


// Main loop
int main()
{
	// Trap signal to clean up
	signal(SIGINT, my_handler);


	// Initialize the codec
	codec_para_t codecContext = { 0 };

	codecContext.stream_type = STREAM_TYPE_ES_VIDEO;
	codecContext.video_type = VFORMAT_H264;
	codecContext.has_video = 1;
	codecContext.noblock = 0;	
	codecContext.am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
	codecContext.am_sysinfo.rate = (96000.0 / (24000.0 / 1001.0));	// 24 fps
	codecContext.am_sysinfo.param = (void*)(SYNC_OUTSIDE);

	int api = codec_init(&codecContext);
	if (api != 0)
	{
		printf("codec_init failed (%x).\n", api);
		exit(1);
	}


	// Open the media file
	int fd = open("test.h264", O_RDONLY);
	if (fd < 0)
	{
		printf("test.h264 could not be opened.");
		exit(1);
	}


	// Blank the framebuffer to allow the
	// video layer to be seen
	init_display();


	// Feed video to the codec
	unsigned char buffer[BUFFER_SIZE];
	isRunning = true;

	while (isRunning)
	{
		// Read the ES video data from the file
		int bytesRead = read(fd, &buffer, BUFFER_SIZE);
		if (bytesRead < 1)
		{
			// Loop the video when the end is reached
			lseek(fd, 0, SEEK_SET);
			if (read(fd, &buffer + bytesRead,
				BUFFER_SIZE - bytesRead) < 1)
			{
				printf("Problem reading file.");
				exit(1);
			}
		}


		// Send the data to the codec
		int api = codec_write(&codecContext, &buffer, BUFFER_SIZE);
		if (api != BUFFER_SIZE)
		{
			printf("codec_write error: %x\n", api);
			codec_reset(&codecContext);
		}
	}


	// Close the codec and media file
	codec_close(&codecContext);
	close(fd);


	// Unblank the framebuffer to restore normal display
	restore_display();

	return 0;
}