
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>


int main(int argc, char** argv){
  if(argc<3){
    printf("Error argv. \n");
    exit(-1);
  }

  int fd;
  if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
    printf("Error opening file. \n");
    exit(-1);
  }
  
  off_t phy_addr = strtoul(argv[1], NULL, 0);
  size_t len = strtoul(argv[2], NULL, 0);
  size_t page_size = getpagesize();  
  off_t offset_in_page = phy_addr & (page_size - 1);

  size_t mapped_size = page_size * ( (offset_in_page + len)/page_size + 1);
  
  void *map_base = mmap(NULL,
			mapped_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			phy_addr & ~(off_t)(page_size - 1));

  if (map_base == MAP_FAILED){
    perror("Memory mapped failed\n");
    exit(-1);
  }

  char* virt_addr = (char*)map_base + offset_in_page;
  for(int i = 0; i < len; ++i)
    printf("%02x ", int(*(virt_addr+i)) );

  close(fd);
  printf("\n");
  return 1;
}
