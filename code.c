#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include<string.h>
#include "ufs.h"





void read_superblock(int fd, super_t *super) {
    // Read the superblock from the file
    ssize_t bytes_read = pread(fd, super, sizeof(super_t), 0);
    if (bytes_read != sizeof(super_t)) {
        perror("Error reading superblock");
        exit(1);
    }
}


void create_root(int fd , super_t *super){
	inode_t root ;
	pread(fd, &root , sizeof(inode_t),super->inode_region_addr*UFS_BLOCK_SIZE) ;
	if(root.type ==2  ){
		return ;
	}
    //writing inode bitmap for root

	unsigned char inode_bitmap_byte  ;
    pread(fd, &inode_bitmap_byte,sizeof(unsigned char), (super->inode_bitmap_addr)*UFS_BLOCK_SIZE);
    inode_bitmap_byte |= (1<<0) ;
	pwrite(fd,&inode_bitmap_byte,sizeof(unsigned char),(super->inode_bitmap_addr)*UFS_BLOCK_SIZE);
    
    //writing inode table  ;
    root.type = 2 ;
    root.size = sizeof(dir_ent_t) ;
    root.direct[0] = 0 ; 
    pwrite(fd , &root , sizeof(inode_t),(super->inode_region_addr)*UFS_BLOCK_SIZE) ;
     
    //writing in data_bitmap for root 
     pwrite(fd,&inode_bitmap_byte,sizeof(unsigned char),(super->data_bitmap_addr)*UFS_BLOCK_SIZE);

    //writing in data region .
    dir_ent_t entry ;
    entry.inum= 0;
    strcpy(entry.name,".");
    pwrite(fd,&entry,sizeof(entry),(super->data_region_addr)*UFS_BLOCK_SIZE);
     
    return ;


}

int find_free_inode(int fd, super_t *super) {
    int total_inodes = super->num_inodes;
    int bits_per_block = 8 * UFS_BLOCK_SIZE;

    // Iterate through each bit in the inode bitmap
    for (int bit_offset = 0; bit_offset < total_inodes; bit_offset++) {
        int byte_idx = bit_offset / 8;
        int bit_idx = bit_offset % 8;

        // Read the byte containing the bit we are interested in
        unsigned char byte;
        pread(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr ) * UFS_BLOCK_SIZE+ byte_idx*sizeof(unsigned char));

        // Check if the bit is clear (not allocated)
        if ((byte & (1 << bit_idx)) == 0) {
            // Mark the bit as allocated
            byte = byte | (1 << bit_idx);
            pwrite(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr ) * UFS_BLOCK_SIZE+ byte_idx*sizeof(unsigned char));
            return bit_offset;
        }
    }
    
    return -1; // No free inode found
}

int find_free_data_block(int fd, super_t *super) {
    int total_data_blocks = super->num_data;
    int bits_per_block = 8 * UFS_BLOCK_SIZE;

    // Iterate through each bit in the data bitmap
    for (int bit_offset = 0; bit_offset < total_data_blocks; bit_offset++) {
        int byte_idx = bit_offset / 8;
        int bit_idx = bit_offset % 8;

        // Read the byte containing the bit we are interested in
        unsigned char byte;
        pread(fd, &byte, sizeof(unsigned char), (super->data_bitmap_addr ) * UFS_BLOCK_SIZE + byte_idx * sizeof(unsigned char));

        // Check if the bit is clear (not allocated)
        if ((byte & (1 << bit_idx)) == 0) {
            // Mark the bit as allocated
            byte = byte |(1 << bit_idx);
            pwrite(fd, &byte, sizeof(unsigned char), (super->data_bitmap_addr ) * UFS_BLOCK_SIZE + byte_idx * sizeof(unsigned char));
            return bit_offset + super->data_region_addr; // Return the actual block number
        }
    }
    
    return -1; // No free data block found
}

int lookup(int fd, super_t *super, char *filename) {
    dir_ent_t entry;
    int max_entries = super->num_inodes; // Adjust to match the number of inodes

    for (int i = 0; i < max_entries; i++) {
        pread(fd, &entry, sizeof(dir_ent_t), (super->data_region_addr +i)* UFS_BLOCK_SIZE );
        if (entry.inum != -1 && strcmp(entry.name, filename) == 0) {
            return entry.inum;
        }
    }
    return -1; 
}

int pi(int fd , super_t *super , int pinum){
    int byte_idx = pinum/8 ;
    int bit_idx = pinum%8 ;
    unsigned char byte_read ;
    pread(fd,&byte_read,sizeof(unsigned char) , super->inode_bitmap_addr*UFS_BLOCK_SIZE+byte_idx*sizeof(unsigned char)) ;
    if((byte_read & (1 << bit_idx)) != 0){
        inode_t check ;
        pread(fd,&check,sizeof(inode_t) , super->inode_region_addr*UFS_BLOCK_SIZE+pinum*sizeof(inode_t)) ;
        if (check.type == 2) 
        {return pinum;}
    }
  return -1 ;
}

int create(int fd , super_t *super ,char* filename , int type , int pinum){
	if(strlen(filename)>=28){
		return -1 ;
	}
    int p_ex = pi(fd,super,pinum);
    if(p_ex!=-1){
        p_ex =-10;
    }else{
        return -2 ;
    }

    inode_t parent_inode ;
    
    pread(fd,&parent_inode,sizeof(parent_inode) , super->inode_region_addr * UFS_BLOCK_SIZE + pinum * sizeof(inode_t));
    for(int i = 0; i<DIRECT_PTRS;i++){
        if(parent_inode.direct[i]==-1){p_ex=i;break;}

    }
    if(p_ex==-1){
        printf("The directory is full.\n");
        return -3 ;
    }
     

	int free_inode = find_free_inode(fd,super) ;
	if(free_inode ==-1){return -1;}
	int free_data_block = find_free_data_block(fd , super) ;
	if(free_data_block==-1){return -1 ;}
	
	inode_t inode ;
	inode.type = type ;
	inode.size = 0 ;
	inode.direct[0] = free_data_block;
	for (int i = 1; i < DIRECT_PTRS; i++) {
        inode.direct[i] = -1;
    }
    pwrite(fd, &inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + free_inode * sizeof(inode_t));
     
    dir_ent_t entry;
    strcpy(entry.name, filename);
    entry.inum = free_inode ;
    pwrite(fd, &entry, sizeof(dir_ent_t), super->data_region_addr * UFS_BLOCK_SIZE+free_data_block *UFS_BLOCK_SIZE); 
 
    
    parent_inode.direct[p_ex] = free_data_block ; 
    pwrite(fd,&parent_inode,sizeof(inode_t),super->inode_region_addr * UFS_BLOCK_SIZE + pinum * sizeof(inode_t));

    return free_inode ;

}

void wrt(int fd , super_t *super, int inode , char *data){
        inode_t wrt_inode ;
        pread(fd,&wrt_inode, sizeof(inode_t), super->inode_region_addr*UFS_BLOCK_SIZE+inode*sizeof(inode_t));
        //if(wrt_inode.type!=1){printf("The requested inode doesn't index a text file.");return;}
        int free_data_index = -1;
        for (int i = 0; i < DIRECT_PTRS; i++) {
            if (wrt_inode.direct[i] == -1) {
              free_data_index = i;
               break;
            }
        }
         if (free_data_index == -1) {
          printf("No available direct pointer in inode %d\n", inode);
          return;
         }
        
        int free_data_block = find_free_data_block(fd , super) ;
        if(free_data_block==-1){printf("No free data block");return ;}
        wrt_inode.direct[free_data_index] = free_data_block; 
        wrt_inode.size+=strlen(data)+1 ;
        pwrite(fd ,data , strlen(data)+1 ,super->data_region_addr*UFS_BLOCK_SIZE+free_data_block*UFS_BLOCK_SIZE); 
        pwrite(fd,&wrt_inode, sizeof(inode_t), super->inode_region_addr*UFS_BLOCK_SIZE+inode*sizeof(inode_t));
        //for debug 
        //char dbg[4096] ;
        //pread(fd,&dbg,strlen(data)+1 ,super->data_region_addr*UFS_BLOCK_SIZE+free_data_block*UFS_BLOCK_SIZE);
        //printf("%s", dbg);
        //
        return ; 

}

void wrt_no_data(int fd , super_t *super , int inode){
    inode_t wrt_inode ;
    pread(fd,&wrt_inode, sizeof(inode_t), super->inode_region_addr*UFS_BLOCK_SIZE+inode*sizeof(inode_t));
    if(wrt_inode.type!=1){printf("The requested inode doesn't index a text file.");return;}


    printf("Please enter the input(make sure it is less than 512 bytes:)\n") ;
    char str[512];
    str[511] = '\0' ;
    fgets(str , 512 , stdin) ;
   // puts(str);
    wrt(fd,super,inode,str);
}


  void read_file(int fd , super_t *super , int inode_num ){
       
  inode_t inode;
    
    pread(fd, &inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode_num * sizeof(inode_t));

    if (inode.type == -1) {
        printf("Inode %d is not allocated\n", inode_num);
        return;
    }

    char buffer[UFS_BLOCK_SIZE];
    buffer[UFS_BLOCK_SIZE-1] = '\0'; 
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (inode.direct[i] != -1) {
            // Read the data block
            pread(fd, buffer, UFS_BLOCK_SIZE, super->data_region_addr*UFS_BLOCK_SIZE+ inode.direct[i] * UFS_BLOCK_SIZE);
            printf("%s", buffer);
            if(i==0){printf("\n");}
        }
    }

    printf("\n");
}

void list_all_files(int fd, super_t *super , int inode){
      inode_t check_inode ;
      pread(fd,&check_inode , sizeof(inode_t) , (super->inode_region_addr) *UFS_BLOCK_SIZE + inode*sizeof(inode_t)) ;
      if(check_inode.type !=2){
        printf("not a directory\n") ;
        return ;
      }
      for(int i=1 ; i < DIRECT_PTRS ;i++){
        if(check_inode.direct[i]!=-1){
            dir_ent_t name ;
            pread(fd,&name,sizeof(dir_ent_t),(super->data_region_addr+check_inode.direct[i])*UFS_BLOCK_SIZE);
            inode_t inode_read ;
            pread(fd,&inode_read,sizeof(inode_t),(super->inode_region_addr)*UFS_BLOCK_SIZE+name.inum*sizeof(inode_t)) ;
            if(inode_read.type==1){
                printf("%-30s%s\n",name.name,"-file");
            }
            else if(inode_read.type==2){
                printf("%-30s%s\n",name.name,"-directory");
            }
        }
      }
return ;
}

void ulink(int fd, super_t *super,char *name, int pinum ) {
    // Use lookup to find the inode number of the file/directory
    int inode_num = lookup(fd, super, name);
    if (inode_num == -1) {
        printf("File or directory not found\n");
        return;
    }

    inode_t inode;
    pread(fd, &inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode_num * sizeof(inode_t));

    // Check if it's a directory and if it's empty
    if (inode.type == 2) {
        int non_empty_count = 0;
        for (int i = 0; i < DIRECT_PTRS; i++) {
            if (inode.direct[i] != -1) {
                non_empty_count++;
            }
        }
        if (non_empty_count > 2) {
            printf("Directory is not empty\n");
            return;
        }
    }

    // Remove the directory entry from the parent directory
    inode_t parent_inode;
    pread(fd, &parent_inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + pinum * sizeof(inode_t));

    dir_ent_t entry;
    int found = 0;
    int entry_offset;
    int entry_block;

    for (int i = 0; i < DIRECT_PTRS && !found; i++) {
        if (parent_inode.direct[i] != -1) {
            entry_block = parent_inode.direct[i];
            for (int j = 0; j < UFS_BLOCK_SIZE / sizeof(dir_ent_t); j++) {
                pread(fd, &entry, sizeof(dir_ent_t), (super->data_region_addr + entry_block) * UFS_BLOCK_SIZE + j * sizeof(dir_ent_t));
                if (entry.inum == inode_num) {
                    found = 1;
                    entry_offset = j * sizeof(dir_ent_t);
                    break;
                }
            }
        }
    }

    if (!found) {
        printf("Directory entry not found\n");
        return;
    }

    // Mark the inode as free
    int byte_idx = inode_num / 8;
    int bit_idx = inode_num % 8;
    unsigned char byte;
    pread(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr) * UFS_BLOCK_SIZE + byte_idx);
    byte &= ~(1 << bit_idx);
    pwrite(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr) * UFS_BLOCK_SIZE + byte_idx);

    // Clear the inode region
    inode_t clear_inode;
    memset(&clear_inode, 0, sizeof(inode_t));
    pwrite(fd, &clear_inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode_num * sizeof(inode_t));

    // Mark the data blocks as free and clear them
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (inode.direct[i] != -1) {
            int data_block = inode.direct[i];
            byte_idx = data_block / 8;
            bit_idx = data_block % 8;
            pread(fd, &byte, sizeof(unsigned char), (super->data_bitmap_addr) * UFS_BLOCK_SIZE + byte_idx);
            byte &= ~(1 << bit_idx);
            pwrite(fd, &byte, sizeof(unsigned char), (super->data_bitmap_addr) * UFS_BLOCK_SIZE + byte_idx);

            char clear_block[UFS_BLOCK_SIZE];
            memset(clear_block, 0, UFS_BLOCK_SIZE);
            pwrite(fd, clear_block, UFS_BLOCK_SIZE, (super->data_region_addr + data_block) * UFS_BLOCK_SIZE);
        }
    }

    // Remove the directory entry
    entry.inum = -1;
    memset(entry.name, 0, sizeof(entry.name));
    pwrite(fd, &entry, sizeof(dir_ent_t), (super->data_region_addr + entry_block) * UFS_BLOCK_SIZE + entry_offset);

    printf("%s unlinked successfully\n", name);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <option> <image_file> <_>\n", argv[0]);
        exit(1);
    }
    
    char *image_file = argv[2];
    
    // Open the file system image file
      int fd = open(image_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Create a super_t structure to hold the superblock data
    super_t super;
    
   
    // Read the superblock
    read_superblock(fd, &super);

    create_root(fd,&super);
    
    if(!strcmp(argv[1],"-rs")){
    // Print the superblock details
    printf("Superblock:\n");
    printf("  Inode bitmap address: %d\n", super.inode_bitmap_addr);
    printf("  Inode bitmap length: %d\n", super.inode_bitmap_len);
    printf("  Data bitmap address: %d\n", super.data_bitmap_addr);
    printf("  Data bitmap length: %d\n", super.data_bitmap_len);
    printf("  Inode region address: %d\n", super.inode_region_addr);
    printf("  Inode region length: %d\n", super.inode_region_len);
    printf("  Data region address: %d\n", super.data_region_addr);
    printf("  Data region length: %d\n", super.data_region_len);
    printf("  Number of inodes: %d\n", super.num_inodes);
    printf("  Number of data blocks: %d\n", super.num_data);}
     
    
    if(!strcmp(argv[1],"-cf")){
        int inode_entry = create(fd ,&super , argv[3] , atoi(argv[4]),atoi(argv[5])) ;
        if(inode_entry==-1){printf("Failed");} 
        else{printf("file created at inode entry %d" , inode_entry) ;}             
    }
  if(!strcmp(argv[1],"-lu")){
      printf("%s is at inode number %d" , argv[3] , lookup(fd , &super , argv[3]));      
  }
   if(!strcmp(argv[1],"-wf")){
      wrt_no_data(fd,&super,atoi(argv[3]));
   }
   if(!strcmp(argv[1] ,"-rf")){
        read_file(fd ,&super , atoi(argv[3]));
   }
   if(!strcmp(argv[1],"-laf")){
      list_all_files(fd,&super,atoi(argv[3]));
   }
   if (!strcmp(argv[1], "-ul")) {
    ulink(fd, &super,argv[3], atoi(argv[4]));
}

    // Close the file descriptor
    close(fd);

    return 0;
}
