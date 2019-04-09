/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define STREQ(a,b) (strcmp(a,b) == 0)

char *repAlg = NULL;
int *physicalMemoryUsed = NULL;
struct disk * d = NULL;

void randAlg(struct page_table*, int);
void fifo(struct page_table*, int);
void custom(struct page_table*, int);
int firstEmpty(struct page_table*);


void page_fault_handler( struct page_table *pt, int page)
{
	printf("page fault on page #%d\n",page);

	// Check if it's already in the page table and has read bit set.

	int frame, bits;

	page_table_get_entry(pt,page,&frame,&bits);

	if (bits & PROT_READ) {
		bits = bits | PROT_WRITE; 
		return;
	}

	if (STREQ(repAlg,"rand")) {
		randAlg(pt, page);
	}
	else if (STREQ(repAlg, "fifo")) {
		fifo(pt, page);
	}
	else if (STREQ(repAlg, "custom")) {
		custom(pt, page);
	}
	else {
		fprintf(stderr, "main.c: invalid replacement algorithm\n");
		exit(1);
	}
	exit(1);
}

int main( int argc, char *argv[] )
{
	srand(time(0)); // set random seed (1 time)
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <alpha|beta|gamma|delta>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	repAlg = malloc(sizeof(argv[3]));
	physicalMemoryUsed = calloc(nframes,sizeof(int));
	if (repAlg == NULL) { // malloc failed
		fprintf(stderr, "main.c: couldn't malloc: %s\n", strerror(errno));
		exit(1);
	}
	repAlg = argv[3];
	const char *program = argv[4];

	struct disk *disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}
	d = disk;

	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	unsigned char *virtmem = page_table_get_virtmem(pt);

	unsigned char *physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"alpha")) {
		alpha_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"beta")) {
		beta_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"gamma")) {
		gamma_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"delta")) {
		delta_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[4]);
		return 1;
	}

	free(repAlg);
	free(physicalMemoryUsed);
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}

void randAlg(struct page_table* pt, int page) {
	int nframes = page_table_get_nframes(pt);
	// random seed
	int randFrameNum;
	int frame, bits;
	int emptyFrame = firstEmpty(pt);
	char buffer[BLOCK_SIZE];
	if (emptyFrame < 0) {
		randFrameNum = rand()%nframes; // random page to evict.
		page_table_get_entry(pt,randFrameNum,&frame,&bits);
		physicalMemoryUsed[randFrameNum] = 0;
		if (bits & PROT_WRITE) {
			// write page to disk
			disk_write(d,randFrameNum,page_table_get_physmem(pt)+PAGE_SIZE*randFrameNum);			 // TODO: how to recall?
		}
			
	} else {
			disk_read(d,emptyFrame,buffer);
			void * pm = page_table_get_physmem(pt);
			void * memdest = memcpy(pm + PAGE_SIZE*emptyFrame, buffer,PAGE_SIZE);
			page_table_set_entry(pt,page,emptyFrame,PROT_READ);	
	} // evict a page and then recall.

	return;
}

void fifo(struct page_table* pt, int page) {
	return;
}

void custom(struct page_table* pt, int page) {
	return;
}

// return pointer to first empty frame in physical memory
int firstEmpty(struct page_table *pt) {
	// iterate through by PAGE_SIZE and find first empty page
	int i;
	for (i = 0; i < page_table_get_nframes(pt); i++) {
		if (!physicalMemoryUsed[i])  {
			physicalMemoryUsed[i] = 1;
			return i;
		}
	}
	return -1; // couldn't find empty frame
}
	
