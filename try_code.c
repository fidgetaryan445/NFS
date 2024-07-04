#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include<string.h>
#include<stdbool.h>
#include "ufs.h"



//int parent_dir = 0 ;
int curr_dir[32] ; 
unsigned int dir_counter;



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

int lookup(int fd, super_t *super, char *filename, int pinum) {
    dir_ent_t entry;
    //int max_entries = super->num_inodes; // Adjust to match the number of inodes
    inode_t parent_inode ;
    pread(fd,&parent_inode,sizeof(inode_t) , super->inode_region_addr *UFS_BLOCK_SIZE + pinum *sizeof(inode_t)) ;
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if(parent_inode.direct[i]!=-1){
        pread(fd, &entry, sizeof(dir_ent_t), (super->data_region_addr +parent_inode.direct[i])* UFS_BLOCK_SIZE );
        if (entry.inum != -1 && strcmp(entry.name, filename) == 0) {
            return entry.inum;
        }
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
    if(wrt_inode.type!=1){printf("The requested inode doesn't index a text file.\n");return;}


    printf("Please enter the input(make sure it is less than 512 bytes):\n") ;
    
    char str[512];
    if (fgets(str, sizeof(str), stdin) == NULL) {
        printf("Error reading input.\n");
        return;
    }

    // Remove the trailing newline character if it exists
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }

    wrt(fd, super, inode, str);
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

char *lookup_addr(int fd, super_t *super, char filename[28], char curr_addr[], int inode) {
    inode_t parent_inode;
    pread(fd, &parent_inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode * sizeof(inode_t));
    
    for (int i = 1; i < DIRECT_PTRS; i++) {
        if (parent_inode.direct[i] != -1) {
            dir_ent_t data_block;
            pread(fd, &data_block, sizeof(dir_ent_t), (super->data_region_addr + parent_inode.direct[i]) * UFS_BLOCK_SIZE);
            
            inode_t data_inode;
            pread(fd, &data_inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + data_block.inum * sizeof(inode_t));
            
            if (strcmp(data_block.name, filename) == 0 ){//&& data_inode.type == 1) {
                strcat(curr_addr, "/");
                strcat(curr_addr, filename);
                return curr_addr;
            } else if (data_inode.type == 2) {
                char temp_addr[1024];  // Adjust the size according to your needs
                strcpy(temp_addr, curr_addr);
                strcat(temp_addr, "/");
                strcat(temp_addr, data_block.name);
                
                char *result = lookup_addr(fd, super, filename, temp_addr, data_block.inum);
                if (strcmp(result, "Failed to find the file") != 0) {
                    strcpy(curr_addr, temp_addr);
                    return curr_addr;
                }
            }
        }
    }
    
    return "Failed to find the file";
}

void help(){
       printf("%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n%-10s%s\n" , "rs" ,"Reads the superblock of our flat file" ,"cf","Allows user to create file in the current directory" ,"lad","Allows the user to search for a file in the entire filesystem","wf","Allows user to chose a file in current directory and then write in it" ,"rf" ,"Allows user to read a file in current directory" ,"ul","Unlinks a file or an empty directory","laf" ,"Lists all files in the current directory" ,"lu" , "Returns the inode number of file in the current directory" , "cd","Allows user to change directory (in forward direction)" ,"gb" , "Allows user to go to previous directory" ,"shutdown" , "Close the emulated filesystem") ;
}

void clear_input_stream() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void ulink(int fd, super_t *super,char *name, int pinum ) {
    // Use lookup to find the inode number of the file/directory
    int inode_num = lookup(fd, super, name,pinum);
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

char* curr_working_dir(int fd,super_t *super,unsigned int x , int arr[32]){
    static char cwd[1024];
    memset(cwd,'\0',sizeof(cwd));
    strcat(cwd,"/");
    for(unsigned int i=1;i<=x;i++){
        inode_t inode; 
        pread(fd,&inode,sizeof(inode_t),super->inode_region_addr * UFS_BLOCK_SIZE + arr[i] *sizeof(inode_t));
        dir_ent_t entry ;
        pread(fd,&entry, sizeof(dir_ent_t), (super->data_region_addr + inode.direct[0])*UFS_BLOCK_SIZE) ;
        //printf("\n%s\n" , entry.name);
        strcat(cwd,entry.name);
        strcat(cwd,"/");
    }
    return cwd ;
}


int main(int argc, char *argv[]) {
    /*if (argc < 3) {
        fprintf(stderr, "usage: %s <option> <image_file> <_>\n", argv[0]);
        exit(1);
    }*/

    char *image_file = argv[1];

    // Open the file system image file
    int fd = open(image_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    printf("Welcome to my file system emulation. This is created by me for my NFS_goes_online summer project\nFor knowing how this file system works please input 'help'\n");

    // Create a super_t structure to hold the superblock data
    super_t super;
    int dir_counter = 0;
    int curr_dir[100]; // Assuming a max depth of 100 for simplicity
    curr_dir[dir_counter] = 0;

    // Read the superblock
    read_superblock(fd, &super);

    create_root(fd, &super);

    while (true) {
        char option[10];
        printf("%-50s\n" , "---------------------------------------------------------------------------------------------------");
        printf("[%s]>" , curr_working_dir(fd,&super,dir_counter,curr_dir)) ;
        scanf("%s", option);
        clear_input_stream(); // Clear the input stream

        if (!strcmp(option, "rs")) {
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
            printf("  Number of data blocks: %d\n", super.num_data);
        }

        if (!strcmp(option, "cf")) {
            char name[28];
            int file_type;
            printf("Please enter the file name and file type (1-text file, 2-directory):\n");
            scanf("%s %d", name, &file_type);
            clear_input_stream(); // Clear the input stream

            if (file_type == 1 || file_type == 2) {
                int inode_entry = create(fd, &super, name, file_type, curr_dir[dir_counter]);
                if (inode_entry < 0) {
                    printf("Failed\n");
                } else {
                    printf("File created at inode entry %d\n", inode_entry);
                }
            } else {
                printf("Please enter a valid File Type (1 or 2)\n");
            }
        }

        if (!strcmp(option, "lu")) {
            printf("Please enter the file name:\n");
            char data[28];
            scanf("%s", data);
            clear_input_stream(); // Clear the input stream

            int x = lookup(fd, &super, data, curr_dir[dir_counter]);
            if (x == -1) {
                printf("File not found\n");
            } else {
                printf("%s is at inode %d\n", data, x);
            }
        }

        if (!strcmp(option, "wf")) {
            printf("Please enter the file you would like to write to:\n");
            char data[28];
            scanf("%s", data);
            clear_input_stream(); // Clear the input stream

            wrt_no_data(fd, &super, lookup(fd, &super, data, curr_dir[dir_counter]));
        }

        if (!strcmp(option, "rf")) {
            printf("Please enter the file you would like to read:\n");
            char data[28];
            scanf("%s", data);
            clear_input_stream(); // Clear the input stream

            read_file(fd, &super, lookup(fd, &super, data, curr_dir[dir_counter]));
        }

        if (!strcmp(option, "laf")) {
            list_all_files(fd, &super, curr_dir[dir_counter]);
        }

        if (!strcmp(option, "cd")) {
            printf("Please enter the name of directory (should be present in current dir):\n");
            char data[28];
            scanf("%s", data);
            clear_input_stream(); // Clear the input stream

            int x = lookup(fd, &super, data, curr_dir[dir_counter]);
            if (x != -1) {
                dir_counter++;
                curr_dir[dir_counter] = x;
            } else {
                printf("There is no such directory.\n");
            }
        }

        if (!strcmp(option, "gb")) {
            dir_counter--;
        }
        
        if(!strcmp(option,"ul")){
            printf("Enter the name of the file to be unlinked:");
            char filename[28];
            scanf("%s" , filename);
            clear_input_stream();
            ulink(fd,&super ,filename,curr_dir[dir_counter]);
        }

        if (!strcmp(option, "lad")) {
            printf("Please enter the name of the file you want to find:\n");
            char data[28];
            scanf("%s", data);
            clear_input_stream(); // Clear the input stream
            char curr_addr[1024]="";
            char *result = lookup_addr(fd, &super, data, curr_addr, 0);

            printf("%s\n", result);
        }
         
        if(!strcmp(option,"help")){
            help() ;
        }
        if (!strcmp(option, "shutdown")) {
            break;
        }
        else{
            printf("No such command\n");
        }
    }

    // Close the file descriptor
    close(fd);

    return 0;
}
