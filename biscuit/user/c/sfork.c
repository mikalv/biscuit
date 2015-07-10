#include <litc.h>

int _main(int argc, char **argv)
{
	ulong sum = 0;
	ulong times = 0;
	while (1) {
		ulong st = rdtsc();
		if (fork() == 0)
			exit(0);
		ulong tot = rdtsc() - st;
		sum += tot;
		times++;
		if (times % 100 == 0)
			while (wait(NULL) > 0);
		if (times % 5000 == 0) {
			printf("%ld cycles/fork (avg)\n", sum/times);
			times = sum = 0;
		}
	}

	return 0;
}

// ms
ulong now()
{
	struct timeval t;
	if (gettimeofday(&t, NULL))
		errx(-1, "gettimeofday");
	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

long jointot(pthread_t t[], const int nthreads)
{
	long total = 0;
	int i;
	for (i = 0; i < nthreads; i++) {
		void *retp;
		if (pthread_join(t[i], &retp))
			errx(-1, "pthread join");
		long ret = (long)retp;
		if (ret < 0)
			errx(-1, "thread failed");
		total += ret;
	}
	return total;
}

static int volatile start;
static int volatile cease;

void *renames(void *idp)
{
	long id = (long)idp;
	char dir[32];

	snprintf(dir, sizeof(dir), "d%ld/", id);

	int ret;
	printf("making %s...\n", dir);
	if ((ret = mkdir(dir, 0700)) < 0)
		err(ret, "mkdir");

	if ((ret = chdir(dir)) < 0)
		err(ret, "chdir");

	while (!start)
		asm volatile("pause":::"memory");

	long total = 0;
	while (!cease) {
		char *o = "old";
		char *n = "new";
		int fd = open(o, O_CREAT | O_RDONLY, 0600);
		if (fd < 0)
			err(fd, "open");
		if ((ret = rename(o, n)) < 0)
			err(ret, "rename");
		total++;
	}
	return (void *)total;
}

void *lookups(void *idp)
{
	while (!start)
		asm volatile("pause":::"memory");

	long total = 0;
	while (!cease) {
		int ret;
		if ((ret = open("NONE", O_RDONLY)) != -ENOENT)
			err(ret, "open: unexpected error");
		total++;
	}
	return (void *)total;
}

int main(int argc, char **argv)
{
	if (argc != 2)
		errx(-1, "usage: %s <num threads>", argv[0]);

	int nthreads = atoi(argv[1]);
	if (nthreads < 0)
		nthreads = 3;
	const int bmsecs = 5;
	printf("using %d threads for %d seconds\n", nthreads, bmsecs);

	pthread_t t[nthreads];
	int i;
	for (i = 0; i < nthreads; i++)
		if (pthread_create(&t[i], NULL, lookups, (void *)(long)i))
			errx(-1, "pthread create");

	ulong st = now();
	start = 1;

	sleep(bmsecs);
	cease = 1;
	ulong beforejoin = now();
	long total = jointot(t, nthreads);
	ulong actual = now() - st;

	printf("ran for %lu ms (slept %lu)\n", actual, beforejoin - st);
	printf("total lookups: %ld\n", total);

	return 0;
}