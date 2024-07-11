#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "udp.h"
#include "ufs.h"

#define BUFFER_SIZE (1000)

void read_superblock(int fd, super_t *super) {
    
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
   

	unsigned char inode_bitmap_byte  ;
    pread(fd, &inode_bitmap_byte,sizeof(unsigned char), (super->inode_bitmap_addr)*UFS_BLOCK_SIZE);
    inode_bitmap_byte |= (1<<0) ;
	pwrite(fd,&inode_bitmap_byte,sizeof(unsigned char),(super->inode_bitmap_addr)*UFS_BLOCK_SIZE);
    
  
    root.type = 2 ;
    root.size = sizeof(dir_ent_t) ;
    root.direct[0] = 0 ; 
    pwrite(fd , &root , sizeof(inode_t),(super->inode_region_addr)*UFS_BLOCK_SIZE) ;
     
   
     pwrite(fd,&inode_bitmap_byte,sizeof(unsigned char),(super->data_bitmap_addr)*UFS_BLOCK_SIZE);

   
    dir_ent_t entry ;
    entry.inum= 0;
    strcpy(entry.name,".");
    pwrite(fd,&entry,sizeof(entry),(super->data_region_addr)*UFS_BLOCK_SIZE);
     
    return ;


}

int find_free_inode(int fd, super_t *super) {
    int total_inodes = super->num_inodes;
    int bits_per_block = 8 * UFS_BLOCK_SIZE;

    
    for (int bit_offset = 0; bit_offset < total_inodes; bit_offset++) {
        int byte_idx = bit_offset / 8;
        int bit_idx = bit_offset % 8;

        
        unsigned char byte;
        pread(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr ) * UFS_BLOCK_SIZE+ byte_idx*sizeof(unsigned char));

        
        if ((byte & (1 << bit_idx)) == 0) {
           
            byte = byte | (1 << bit_idx);
            pwrite(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr ) * UFS_BLOCK_SIZE+ byte_idx*sizeof(unsigned char));
            return bit_offset;
        }
    }
    
    return -1; 
}

int find_free_data_block(int fd, super_t *super) {
    int total_data_blocks = super->num_data;
    int bits_per_block = 8 * UFS_BLOCK_SIZE;

   
    for (int bit_offset = 0; bit_offset < total_data_blocks; bit_offset++) {
        int byte_idx = bit_offset / 8;
        int bit_idx = bit_offset % 8;

       
        unsigned char byte;
        pread(fd, &byte, sizeof(unsigned char), (super->data_bitmap_addr ) * UFS_BLOCK_SIZE + byte_idx * sizeof(unsigned char));

        
        if ((byte & (1 << bit_idx)) == 0) {
            
            byte = byte |(1 << bit_idx);
            pwrite(fd, &byte, sizeof(unsigned char), (super->data_bitmap_addr ) * UFS_BLOCK_SIZE + byte_idx * sizeof(unsigned char));
            return bit_offset + super->data_region_addr; 
        }
    }
    
    return -1; // No free data block found
}

int lookup(int fd, super_t *super, char *filename) {
    dir_ent_t entry;
    int max_entries = super->num_inodes; 

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


  void read_file(int fd, super_t *super, int inode_num, char *response) {
    inode_t inode;
    pread(fd, &inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode_num * sizeof(inode_t));

    if (inode.type == -1) {
        snprintf(response, BUFFER_SIZE, "Inode %d is not allocated\n", inode_num);
        return;
    }

    char buffer[UFS_BLOCK_SIZE + 1];
    buffer[UFS_BLOCK_SIZE] = '\0'; // Ensure null termination
    char temp_buffer[BUFFER_SIZE] = ""; // Temporary buffer to hold the output

    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (inode.direct[i] != -1) {
            // Read the data block
            pread(fd, buffer, UFS_BLOCK_SIZE, super->data_region_addr * UFS_BLOCK_SIZE + inode.direct[i] * UFS_BLOCK_SIZE);
            snprintf(temp_buffer + strlen(temp_buffer), BUFFER_SIZE - strlen(temp_buffer), "%s", buffer);
            if (i == 0) {
                snprintf(temp_buffer + strlen(temp_buffer), BUFFER_SIZE - strlen(temp_buffer), "\n");
            }
        }
    }

    snprintf(temp_buffer + strlen(temp_buffer), BUFFER_SIZE - strlen(temp_buffer), "\n");
    strncpy(response, temp_buffer, BUFFER_SIZE); // Copy the output to the response buffer
    return;
}


void list_all_files(int fd, super_t *super, int inode, char *response) {
    inode_t check_inode;
    pread(fd, &check_inode, sizeof(inode_t), (super->inode_region_addr) * UFS_BLOCK_SIZE + inode * sizeof(inode_t));
    
    if (check_inode.type != 2) {
        snprintf(response, BUFFER_SIZE, "Not a directory\n");
        return;
    }
    
    char temp_buffer[BUFFER_SIZE] = ""; // Temporary buffer to hold the output
    for (int i = 1; i < DIRECT_PTRS; i++) {
        if (check_inode.direct[i] != -1) {
            dir_ent_t name;
            pread(fd, &name, sizeof(dir_ent_t), (super->data_region_addr + check_inode.direct[i]) * UFS_BLOCK_SIZE);
            inode_t inode_read;
            pread(fd, &inode_read, sizeof(inode_t), (super->inode_region_addr) * UFS_BLOCK_SIZE + name.inum * sizeof(inode_t));
            if (inode_read.type == 1) {
                snprintf(temp_buffer + strlen(temp_buffer), BUFFER_SIZE - strlen(temp_buffer), "%-30s%-5d%s\n", name.name,lookup(fd,super,name.name) , "-file");
            } else if (inode_read.type == 2) {
                snprintf(temp_buffer + strlen(temp_buffer), BUFFER_SIZE - strlen(temp_buffer), "%-30s%-5d%s\n", name.name,lookup(fd,super,name.name),"-directory");
            }
        }
    }
    
    strncpy(response, temp_buffer, BUFFER_SIZE); 
    return;
}


void ulink(int fd, super_t *super,char *name, int pinum ) {
    
    int inode_num = lookup(fd, super, name);
    if (inode_num == -1) {
        printf("File or directory not found\n");
        return;
    }

    inode_t inode;
    pread(fd, &inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode_num * sizeof(inode_t));

 
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

   
    int byte_idx = inode_num / 8;
    int bit_idx = inode_num % 8;
    unsigned char byte;
    pread(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr) * UFS_BLOCK_SIZE + byte_idx);
    byte &= ~(1 << bit_idx);
    pwrite(fd, &byte, sizeof(unsigned char), (super->inode_bitmap_addr) * UFS_BLOCK_SIZE + byte_idx);

  
    inode_t clear_inode;
    memset(&clear_inode, 0, sizeof(inode_t));
    pwrite(fd, &clear_inode, sizeof(inode_t), super->inode_region_addr * UFS_BLOCK_SIZE + inode_num * sizeof(inode_t));

   
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

    
    entry.inum = -1;
    memset(entry.name, 0, sizeof(entry.name));
    pwrite(fd, &entry, sizeof(dir_ent_t), (super->data_region_addr + entry_block) * UFS_BLOCK_SIZE + entry_offset);

    printf("%s unlinked successfully\n", name);
}


void process_request(char *request, char *response) {
  
    char *token = strtok(request, " ");
    char *argv[259];
    int argc = 1;
    argv[258]= '\0' ;
    while (token != NULL) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    argv[argc] = '\0' ;
    int length = argc - 4 ;
    length*=2 ;

    char *image_file = argv[2];
    
   
    int fd = open(image_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open");
        strcpy(response, "Error opening file");
        return;
    }

  
    super_t super;
    read_superblock(fd, &super);
    create_root(fd, &super);

    if (strcmp(argv[1], "-rs") == 0) {
        sprintf(response, "Superblock:\n  Inode bitmap address: %d\n  Inode bitmap length: %d\n  Data bitmap address: %d\n  Data bitmap length: %d\n  Inode region address: %d\n  Inode region length: %d\n  Data region address: %d\n  Data region length: %d\n  Number of inodes: %d\n  Number of data blocks: %d\n",
                super.inode_bitmap_addr, super.inode_bitmap_len, super.data_bitmap_addr, super.data_bitmap_len, super.inode_region_addr, super.inode_region_len, super.data_region_addr, super.data_region_len, super.num_inodes, super.num_data);
    } else if (strcmp(argv[1], "-cf") == 0) {
        int inode_entry = create(fd, &super, argv[3], atoi(argv[4]), atoi(argv[5]));
        if (inode_entry == -1) {
            strcpy(response, "Failed to create file");
        } else {
            sprintf(response, "File created at inode entry %d", inode_entry);
        }
    } else if (strcmp(argv[1], "-lu") == 0) {
        sprintf(response, "%s is at inode number %d", argv[3], lookup(fd, &super, argv[3]));
    } 
    else if (strcmp(argv[1], "-wf") == 0) {
    if (argc < 5) {
        fprintf(stderr, "Insufficient arguments for -wf option\n");
        return; 
    }

    int inode_num = atoi(argv[3]);

   
    int total_length = 0;
    for (int i = 4; i < argc; ++i) {
        total_length += strlen(argv[i]) + 1; 
    }

    // Allocate memory for data
    char *data = (char *)malloc(total_length * sizeof(char));
    if (data == NULL) {
        perror("Memory allocation failed");
        return;
    }

  
    int current_position = 0;
    for (int i = 4; i < argc; ++i) {
        strcpy(data + current_position, argv[i]);
        current_position += strlen(argv[i]);
        
            data[current_position] = ' ';
            current_position++;
     
    }
    data[current_position] = '\0'; 
    wrt(fd, &super, inode_num, data);

    free(data);
    strcpy(response, "Write successful");
}
 else if (strcmp(argv[1], "-rf") == 0) {
        char buf[BUFFER_SIZE];
        read_file(fd, &super, atoi(argv[3]),response);
        //strcpy(response, buf);
    } else if (strcmp(argv[1], "-laf") == 0) {
        list_all_files(fd, &super, atoi(argv[3]),response);
        //strcpy(response, "Listed all files");
    } else if (strcmp(argv[1], "-ul") == 0) {
        ulink(fd, &super, argv[3], atoi(argv[4]));
        strcpy(response, "Unlink complete");
    } else {
        strcpy(response, "No such command found");
    }

   
    close(fd);
}

int main(int argc, char *argv[]) {
    int sd = UDP_Open(10000);
    assert(sd > -1);
    while (1) {
        struct sockaddr_in addr;
        char message[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0) {
            process_request(message, response);
            rc = UDP_Write(sd, &addr, response, BUFFER_SIZE);
            printf("server:: reply\n");
        }
    }
    return 0;
}
