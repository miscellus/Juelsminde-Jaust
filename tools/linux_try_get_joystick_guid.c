#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/uinput.h>

#include <stdlib.h>
#include <stdio.h>

#define DIE(...) (fprintf(stderr, __VA_ARGS__), exit(1))

int main(int argc, char const *argv[])
{
	// const char *path = "/dev/input/by-id/usb-PowerA_Controller_00005A5578AFA931-event-joystick";
	const char *path = "/dev/input/by-id/usb-Microsoft_Corporation._Controller_363EF876-event-joystick";
	int fd = open(path, O_RDONLY | O_NONBLOCK);
	
	if (fd == -1) {
		close(fd);
		DIE("Could not open device");
	}

	struct input_id id;

	if (ioctl(fd, EVIOCGID, &id) < 0) {
		close(fd);
		DIE("Could not get id of device");
	}

	char guid[33] = "";
	
	sprintf(guid, "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
		id.bustype & 0xff, id.bustype >> 8,
		id.vendor & 0xff,  id.vendor >> 8,
		id.product & 0xff, id.product >> 8,
		id.version & 0xff, id.version >> 8);

	printf("GUID: %.32s\n", guid);


	close(fd);
	return 0;
}