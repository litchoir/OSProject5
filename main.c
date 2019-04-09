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

void randAlg(struct page_table*, int);
void fifo(struct page_table*, int);
void custom(struct page_table*, int);
int *firstEmpty(struct page_table*);


void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);

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
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}

void randAlg(struct page_table* pt, int page) {
	int nframes = page_table_get_nframes(pt);
	// random seed
	int randFrameNum;
	randFrameNum = rand()%nframes;
	int *emptyFrame = firstEmpty(pt);
	printf("emptyFrame: %i\n", *emptyFrame);
	return;
}

void fifo(struct page_table* pt, int page) {
	return;
}

void custom(struct page_table* pt, int page) {
	return;
}

// return pointer to first empty frame in physical memory
int *firstEmpty(struct page_table *pt) {
	// iterate through by PAGE_SIZE and find first empty page
	char* physmem = page_table_get_physmem(pt);
	int i;	
	for (i = 0; i < page_table_get_nframes(pt); i+=PAGE_SIZE) {
		if (physmem[i] == NULL) {
			return physmem+i;
		}
	}
	return NULL; // couldn't find empty frame
}
	
