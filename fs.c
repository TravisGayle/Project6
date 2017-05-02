
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
int ismounted = 0;

int fs_format(){

	int node;
	int block = 1;
	int inodeBlocks;
	int dNode;
	int oneTenthBlock = .1 * disk_size();
	inodeBlocks = oneTenthBlock + 1;


	// attempt to format an already-mounted disk does nothing and returns failure.
	if(ismounted){
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

	disk_read(0,block.data);

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
						if(block.pointers[idBlock] != 0){
							printf(" %d",block.pointers[idBlock]);
						}

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

	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
