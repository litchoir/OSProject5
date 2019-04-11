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

int nPageFaults = 0;
int nDiskReads = 0;
int nDiskWrites = 0;


typedef struct Node {
	int frame;
	struct Node * next;
} frameNode;

frameNode * f = NULL;

void replacementAlg(struct page_table*, int);
int randomAlg(struct page_table*);
void customAlg(struct page_table*, int);
int firstEmpty(struct page_table*);
void clear_bits_by_frame(struct page_table*, int);
int get_page_by_frame(struct page_table *, int);
void pushNode(int);
int popNode();

void page_fault_handler( struct page_table *pt, int page)
{
	printf("\n\npage fault on page #%d\n\n",page);
	nPageFaults++;

	// Check if it's already in the page table and has read bit set.

	int frame, bits;

	printf("\n\n\n");

	page_table_get_entry(pt,page,&frame,&bits);
	if (bits & PROT_READ) {	
		bits = bits | PROT_WRITE;
		page_table_set_entry(pt, page, frame, bits);
		page_table_print(pt);	
		return;
	}

	replacementAlg(pt,page);

	page_table_print(pt);	
	return;
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
	repAlg = malloc(100*sizeof(char));
	physicalMemoryUsed = calloc(nframes,sizeof(int));
	if (repAlg == NULL) { // malloc failed
		fprintf(stderr, "main.c: couldn't malloc: %s\n", strerror(errno));
		exit(1);
	}
	strcpy(repAlg, argv[3]);

	if (!STREQ(repAlg,"fifo") && !STREQ(repAlg, "rand") && !STREQ(repAlg,"custom")) {
		fprintf(stderr, "main.c: invalid replacement algorithm\n");
		exit(1);
	}	

	const char *program = argv[4];

	f = malloc(sizeof(frameNode)); // mallocs the starting Node for the head of the list 
	f->next = NULL;
	f->frame = -1;

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
	frameNode * fr = f->next;
	while (fr) {
		 free(f);
		 f = fr;
		 fr = fr->next;
	}
	free(f);

	free(physicalMemoryUsed);
	page_table_delete(pt);
	disk_close(disk);

	printf("# page faults: %d;  # disk reads: %d;  # disk writes: %d\n",nPageFaults,nDiskReads,nDiskWrites);

	return 0;
}

void pushNode(int frameNo) {
	if (f->frame == -1) {
		f->frame = frameNo;	
		return;
	}
	frameNode * curr = f;
	while (curr->next) {
		curr=curr->next;
	}
	curr->next = malloc(sizeof(frameNode));
	curr->next->frame = frameNo;
	curr->next->next = NULL;	
}

int popNode() {
	int returnVal = f->frame;
	if (f->next) f = f->next;
	else f->frame = -1;
	return returnVal;
}


void replacementAlg(struct page_table* pt, int page) {
	int frameToReplace;
	int frame, bits;
	int pageToGet;
	int emptyFrame = firstEmpty(pt);
	char buffer[BLOCK_SIZE];

	if (emptyFrame < 0) { // no empty page table entries		
		 // random frame to evict.	
		if (STREQ(repAlg,"rand")) frameToReplace = randomAlg(pt);
		else if (STREQ(repAlg, "fifo")) frameToReplace = popNode();
		pageToGet = get_page_by_frame(pt, frameToReplace);
		page_table_get_entry(pt,pageToGet,&frame,&bits);
		physicalMemoryUsed[frameToReplace] = 0;
		if (bits & PROT_WRITE) {
			// write page to disk
			disk_write(d,frameToReplace,page_table_get_physmem(pt)+PAGE_SIZE*frameToReplace);
			nDiskWrites++;	
		}
		emptyFrame = frameToReplace;
		// clear bits on the evicted frame in page table
		clear_bits_by_frame(pt, frameToReplace);
	}

	if (STREQ(repAlg,"fifo")) pushNode(emptyFrame);
		
	// write to empty frame from disk
	disk_read(d,emptyFrame,buffer);
	nDiskReads++;
	void * pm = page_table_get_physmem(pt);
	void * memdest = memcpy(pm + PAGE_SIZE*emptyFrame, buffer,PAGE_SIZE);
	// set corresponding page table entry
	page_table_set_entry(pt,page,emptyFrame,PROT_READ);
	physicalMemoryUsed[emptyFrame] = 1;	 
	return;
}

int randomAlg(struct page_table * pt) {
	int nframes = page_table_get_nframes(pt);
	return rand() % nframes; 
}


void customAlg(struct page_table* pt, int page) {
	return;
}

// return number of first empty frame in physical memory
int firstEmpty(struct page_table *pt) {
	// iterate through and find first empty frame
	int i;
	for (i = 0; i < page_table_get_nframes(pt); i++) {
		if (!physicalMemoryUsed[i])  {
			//physicalMemoryUsed[i] = 1;
			printf("empty frame found: %d\n",i);
			return i;
		}
	}
	return -1; // couldn't find empty frame
}

// clear bits in page table based on frame number
void clear_bits_by_frame(struct page_table *pt, int frame) {
	// iterate through page table to find corresponding entry
	int page;
	int currFrame;
	int bits;
	for (page = 0; page < page_table_get_npages(pt); page++) {
		page_table_get_entry(pt, page, &currFrame, &bits);
		if (currFrame == frame && bits) {
			page_table_set_entry(pt, page, 0, 0); // set bits to 0
		}
	}
}

// return a page that's currently mapped to frame
int get_page_by_frame(struct page_table *pt, int frame) {
	int page;
	int currFrame;
	int bits;
	for (page = 0; page < page_table_get_npages(pt); page++) {
		page_table_get_entry(pt, page, &currFrame, &bits);
		if (currFrame == frame) {
			return currFrame;
		}
	}
	return -1;
}
