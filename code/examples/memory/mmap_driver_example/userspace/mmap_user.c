#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#define DEV_PATH "/dev/mymmap"
#define MAP_SIZE 4096

static void hexdump_head(const unsigned char *p, size_t n)
{
	size_t i;
	for (i = 0; i < n; i++) {
		printf("%02x ", p[i]);
		if ((i + 1) % 16 == 0)
			printf("\n");
	}
	if (n % 16)
		printf("\n");
}

int main(void)
{
	int fd;
	void *addr;
	char *p;

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	addr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	p = (char *)addr;

	printf("=== read from mapped page (as string) ===\n");
	printf("%s\n", p);

	printf("=== hexdump head (64 bytes) ===\n");
	hexdump_head((unsigned char *)p, 64);

	printf("=== write into mapped page ===\n");
	snprintf(p, MAP_SIZE, "hello from userspace via mmap (MAP_SHARED)\n");

	/*
	 * MAP_SHARED 通常不需要 msync 才能讓 kernel 看到內容，
	 * 但寫入後可用 dmesg / read() 觀察。
	 */
	printf("write done.\n");

	munmap(addr, MAP_SIZE);
	close(fd);
	return 0;
}

