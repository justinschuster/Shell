#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{

  int pid;
  int i;
  int count = 5;
  long sleep = 1000; // 1 second. Unit of sleep: millisecond
  int c;
 
  pid = getpid();
  while ((c = getopt(argc, argv, "s:c:")) != -1) {
    switch (c) {
    case 's':
      sleep = (long)atoi(optarg);
      break;

    case 'c':
      count = atoi(optarg);
      break;
      
    default:
      fprintf(stderr, "Usage: %s [-s <sleeping time between two messages>] [-c <# messages>]\n", argv[0]);
      exit(1);
    }
  }

  for (i = 0; i < count; i++) {
      printf("%d sleeping for %ld milliseconds\n", pid, sleep);
      usleep(sleep * 1000);
  }

  printf("Process %d completed\n", pid);
}