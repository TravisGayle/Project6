
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define DISK_BLOCK_SIZE    4096
#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024
#define SUCCESS    1
#define FAILURE    0

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

union fs_block fsmb;
int isMounted = 0;
int numInodes;
int *inodeTable;
int *bitmap;
int bitmapSize;

void inode_load(int inumber, struct fs_inode *inode) {
	union fs_block block;
	disk_read((int) (inumber / INODES_PER_BLOCK) + 1, block.data);

	*inode = block.inode[inumber % INODES_PER_BLOCK];
}

void inode_save(int inumber, struct fs_inode *inode) {
	union fs_block block;
	disk_read((int) (inumber / INODES_PER_BLOCK) + 1, block.data);

	block.inode[inumber%INODES_PER_BLOCK] = *inode;
	disk_write((int) (inumber / INODES_PER_BLOCK) + 1, block.data);
}

int fs_format(){

	int node;
	int block = 1;
	int inodeBlocks;
	int dNode;
	int oneTenthBlock = .1 * disk_size();
	inodeBlocks = oneTenthBlock + 1;


	// attempt to format an already-mounted disk does nothing and returns failure.
	if(isMounted){
		printf("the disk has already been mounted\n");
		return FAILURE;
	}

	fsmb.super.magic = FS_MAGIC;
	fsmb.super.nblocks = disk_size();
	fsmb.super.ninodeblocks = inodeBlocks;
	fsmb.super.ninodes = INODES_PER_BLOCK * 5;
	disk_write(0, fsmb.data);

	//make all the nodes invalid and set size equal to 0
	while(block < inodeBlocks + 1) {
		for(node = 0; node < INODES_PER_BLOCK; node++) {
			fsmb.inode[node].isvalid = 0;
			fsmb.inode[node].size = 0;
			for(dNode = 0; dNode < POINTERS_PER_INODE; dNode++){
				fsmb.inode[node].direct[dNode] = 0;
				fsmb.inode[node].indirect = 0;
			}//end of for loop
		} //end of for loop

		disk_write(block, fsmb.data);
		block++;
	} //end of while loop

	return SUCCESS;
}

void fs_debug(){
	union fs_block block;

	disk_read(0, block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	int numBlocks = 1;
	while(numBlocks <= block.super.nblocks){
		int node;

		disk_read(numBlocks, block.data); //read the block, for each new block we read block and store in block's data
		for(node = 0; node < INODES_PER_BLOCK; node++){
			if (block.inode[node].isvalid == 1){ //check to see if the node is or has been used and has something in it
				printf("inode %d:\n",node);
				printf("    size: %d bytes\n", block.inode[node].size);
				printf("    direct blocks: ");

				int dNode; //direct block node

				for(dNode = 0; dNode < POINTERS_PER_INODE; dNode++) {
					if(block.inode[node].direct[dNode] != 0)
						printf(" %d", block.inode[node].direct[dNode]);
				}
				printf("\n");
				int idNode = block.inode[node].indirect;
				if(block.inode[node].indirect != 0) {

					printf("    indirect block: %d\n", idNode);

					disk_read(idNode, block.data);
					int idBlock;
					printf("    indirect data blocks: ");
					for(idBlock = 0; idBlock < POINTERS_PER_BLOCK; idBlock++) {
						if(block.pointers[idBlock] != 0)
							printf(" %d",block.pointers[idBlock]);

					}
					printf("\n");

					disk_read(numBlocks,block.data);
				} //end of if statement


			} //end of for loop

		} //end of while loop
		numBlocks++;
	} //end of void function
}


int fs_mount()
{
	// attempt to mount an already-mounted disk does nothing and returns failure.
	if (isMounted) {
		printf("the disk has already been mounted\n");
		return 0;
	}

	union fs_block block;
	disk_read(0, block.data);
	int i, j, k;

	numInodes = block.super.ninodeblocks * INODES_PER_BLOCK;		// initialize inode table
	inodeTable = malloc(numInodes * sizeof(int));
	for (i = 0; i < numInodes; i++)			// fill inode table
		inodeTable[i] = 0;

	bitmapSize = block.super.nblocks;		// initialize bitmap
	bitmap = malloc(bitmapSize * sizeof(int));
	for (i = 0; i < bitmapSize; i++)		// fill bitmap
		bitmap[i] = 0;

	bitmap[0] = 1;		// set super block to 1
	int numInodeBlocks  = block.super.ninodeblocks;

	for (i = 1; i <= numInodeBlocks; i++) {			// loop through blocks
		bitmap[i] = 1;
		for (j = 0; j < INODES_PER_BLOCK; j++) {	// loop through inodes in each block
			disk_read(i, block.data);

			if (block.inode[j].isvalid) {			// ensures block is not invalid or empty
				inodeTable[j + ((i - 1) * INODES_PER_BLOCK)] = 1;

				for (k = 0; k < POINTERS_PER_INODE; k++) {	// loop through inode pointers
					if (block.inode[j].direct[k])			// print direct blocks
						bitmap[block.inode[j].direct[k]] = 1;
				}

				if (block.inode[j].indirect) {
					bitmap[block.inode[j].indirect] = 1;
					disk_read(block.inode[j].indirect, block.data);
					for (k = 0; k < POINTERS_PER_BLOCK; k++) {	// loop through block pointers
						if (block.pointers[k])
							bitmap[block.pointers[k]] = 1;
					}
				}
			}
		}
	}

	isMounted = 1;
	return 1;
}

int fs_create()
{
	if (!isMounted) {
		printf("please mount the disk first\n");
		return 0;
	}

	struct fs_inode inode;
	int i, j;

	for (i = 1; i < numInodes; i++) {	// starts at 1 b/c 0 is returned on failure
		if (inodeTable[i] == 0) {
			inode_load(i, &inode);
			inode.isvalid = 1;
			inode.size = 0;

			for (j = 0; j < POINTERS_PER_INODE; j++)
				inode.direct[j] = 0;

			inode.indirect = 0;
			inode_save(i, &inode);

			inodeTable[i] = 1;
			return i;
		}
	}

	return 0;	// inodeTable was full
}

int fs_delete( int inumber )
{
	if (!isMounted) {
		printf("please mount the disk first\n");
		return 0;
	}

	union fs_block block;
	struct fs_inode inode;
	int i;

	disk_read(0, block.data);

	if (inumber < 0 || inumber > block.super.ninodes)
		return 0;

	inode_load(inumber, &inode);
	if (inode.isvalid == 0)
		return 1;

	inode.isvalid = 0;
	inode.size = 0;

	inode_save(inumber, &inode);

	for (i = 0; i < POINTERS_PER_INODE; i++) {		// free direct pointers from bitmap
		if (inode.direct[i])
			bitmap[inode.direct[i]] = 0;
	}

	if (inode.indirect) {
		disk_read(inode.indirect, block.data);

		for (i = 0; i < POINTERS_PER_BLOCK; i++) {
			if (block.pointers[i])
				bitmap[block.pointers[i]] = 0;
		}
	}

	return 1;
}

int fs_getsize( int inumber )
{
	if (!isMounted) {
		printf("please mount the disk first\n");
		return 0;
	}

	union fs_block block;
	struct fs_inode inode;

	disk_read(0, block.data);

	if (inumber < 0 || inumber > block.super.ninodes)
		return -1;

	inode_load(inumber, &inode);

	if (inode.isvalid)
		return inode.size;

	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	if (!isMounted) {
		printf("please mount the disk first\n");
		return 0;
	}

	union fs_block block;
	struct fs_inode inode;

	disk_read(0, block.data);
	
	if (inumber < 0 || inumber > block.super.ninodes)
		return 0;

	numInodes = (block.super.ninodeblocks * INODES_PER_BLOCK);
 
	int i, j;
	int currbyte = 0, first = 0;
	inode_load(inumber, &inode);

	if (inode.isvalid == 0)
		return 0;

	if(offset >= inode.size)
		return 0;

	int startBlock = (int)(offset / DISK_BLOCK_SIZE);
	int curroffset = offset % 4096;

	for (i = startBlock; i < POINTERS_PER_INODE; i++) {
		if (inode.direct[i]) {
			if (first == 0) {
				disk_read(inode.direct[i], block.data);
				for (j = 0; j+curroffset < DISK_BLOCK_SIZE; j++) {
					if (block.data[j+curroffset]) {
						data[currbyte] = block.data[j+curroffset];
						currbyte++;
						if (currbyte+offset >= inode.size) {
							return currbyte;
						}
					}
					else {
						return currbyte;
					}
					if (currbyte == length) {
						return currbyte;
					}
				} 
				first = 1;
			} else {
				disk_read(inode.direct[i], block.data);
				for (j = 0; j < DISK_BLOCK_SIZE; j++){
					if(block.data[j]){
						data[currbyte] = block.data[j];
						currbyte++;
						if (currbyte+offset >= inode.size)
							return currbyte;

					} else
						return currbyte;

					if (currbyte == length)
						return currbyte;
				} 
			}
		}
	}

	//Indirect nodes here
	union fs_block indirectBlock;
	int startIndirect = startBlock - 5;

	if (inode.indirect) {
		disk_read(inode.indirect, indirectBlock.data);
		for (i = startIndirect; i < POINTERS_PER_BLOCK; i++) {
			if (indirectBlock.pointers[i]) {
				disk_read(indirectBlock.pointers[i], block.data);
				for(j = 0; j < DISK_BLOCK_SIZE; j++) {
					if (block.data[j]) {
						data[currbyte] = block.data[j];
						currbyte++;
						if (currbyte+offset >= inode.size)
							return currbyte;
					} else
						return currbyte;

					if (currbyte == length)
						return currbyte;
				} 
			}
		}
	}
	
	return currbyte;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	if (!isMounted) {
		printf("please mount the disk first\n");
		return 0;
	}

	union fs_block block;
	struct fs_inode inode;
	
	// read the superblock and do checks
	disk_read(0, block.data);
	
	if (inumber < 0 || inumber > block.super.ninodes)
		return 0;

	numInodes = (block.super.ninodeblocks*INODES_PER_BLOCK);
	int nonDiskBlocks = (block.super.ninodeblocks+1);


	int i, j, k;
	int currbyte = 0, first = 0;
	inode_load(inumber, &inode);
	if (inode.isvalid == 0)
		return 0;

	if (inode.isvalid == 1 && inode.size > 0) {
		for (i = 0; i < POINTERS_PER_INODE; i++) {
			if (inode.direct[i])
				bitmap[inode.direct[i]] = 0;
		}
		if (inode.indirect) {
			disk_read(inode.indirect, block.data);
			bitmap[inode.indirect] = 0;
			for (j = 0; j < POINTERS_PER_BLOCK; j++) {
				if (block.pointers[j]) {
					bitmap[block.pointers[j]] = 0;
				}
			}
		}
	}

	int startBlock = (int)(offset/DISK_BLOCK_SIZE);
	int curroffset = offset%4096;
	for (i = startBlock; i < POINTERS_PER_INODE; i++) {
		// go through bitmap to look for empty block
		for (k = nonDiskBlocks; k < bitmapSize; k++) {
			if (bitmap[k]==0) {
				inode.direct[i] = k;
				bitmap[k] = 1;
				break;
			}
		}
		if (first == 0) {
			disk_read(inode.direct[i], block.data);
			for (j = 0; j+curroffset < DISK_BLOCK_SIZE; j++) {
				block.data[j+curroffset] = data[currbyte];
				currbyte++;
				if (currbyte == length) {
					disk_write(inode.direct[i], block.data);
					inode.size = currbyte + offset;
					inode_save(inumber, &inode);
					return currbyte;
				}
			} 
			first = 1;
			disk_write(inode.direct[i], block.data);
		} else {
			disk_read(inode.direct[i], block.data);
			for (j = 0; j < DISK_BLOCK_SIZE; j++) {
				block.data[j] = data[currbyte]; 
				currbyte++;
				if (currbyte == length) {
					disk_write(inode.direct[i], block.data);
					inode.size = currbyte + offset;
					inode_save(inumber, &inode);
					return currbyte;
				}
			} 
			disk_write(inode.direct[i], block.data);
		}
	}

	//Indirect nodes here
	union fs_block indirectBlock;
	// go through bitmap to look for empty block
	for (k = nonDiskBlocks; k < bitmapSize; k++) {
		if (!bitmap[k]) {
			inode.indirect = k;
			bitmap[k] = 1;
			break;
		}
	}

	int startIndirect = startBlock - 5;

	if (inode.indirect) {
		disk_read(inode.indirect, indirectBlock.data);
		for (i = startIndirect; i < POINTERS_PER_BLOCK; i++) {
			// go through bitmap to look for empty block
			for (k = nonDiskBlocks; k < bitmapSize; k++) {
				if (!bitmap[k]) {
					indirectBlock.pointers[i] = k;
					bitmap[k] = 1;
					break;
				}
			}

			disk_read(indirectBlock.pointers[i], block.data);
			for (j = 0; j < DISK_BLOCK_SIZE; j++) {
				block.data[j] = data[currbyte]; 
				currbyte++;
				if (currbyte == length){
					disk_write(indirectBlock.pointers[i], block.data);
					inode.size = currbyte + offset;
					inode_save(inumber, &inode);
					return currbyte;
				}
			} 
			disk_write(indirectBlock.pointers[i], block.data);
		}
		disk_write(inode.indirect, indirectBlock.data);
	}

	// update inode's size
	inode.size = currbyte + offset;
	inode_save(inumber, &inode);
	return currbyte;
}