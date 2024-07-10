# GUIDE
Here I will explain the steps you can follow to try out my emulated filesystem and a Network File System based on the same .

## Emulated File System 
The emulated file system folder consists of two main `C` codes, `code.c` & `try_code.c` , and other essential and header files . The `code.c` consists of the code and functions around which I have written the wrapper for creating the NFS.
`try_code.c` is the code that consists of all the functionality for my emulated file system .

**Steps to try out the emulated file system :**
 1) Give `build.sh` & `run.sh` executable permission.
 2) When using th file system for the first time you must run `build.sh` . For further use `run.sh` must be used .
 
***#NOTE:** The defualt name for the flat is set to `hello_world.fs` in both `build.sh` and `run.sh`. You can change it as per your liking.*

**HELP :**
| Function  | Functionality                                             |
|-----------|------------------------------------------------------------|
| rs        | Reads the superblock of our flat file                      |
| cf        | Allows user to create file in the current directory        |
| lad       | Allows the user to search for a file in the entire filesystem |
| wf        | Allows user to choose a file in the current directory and then write in it |
| rf        | Allows user to read a file in the current directory        |
| ul        | Unlinks a file or an empty directory                       |
| laf       | Lists all files in the current directory                   |
| lu        | Returns the inode number of a file in the current directory |
| cd        | Allows user to change directory (in forward direction)     |
| gb        | Allows user to go to the previous directory                |
| shutdown  | Closes the emulated filesystem                             |

## Network File System 
The folder consists of `client.c` , `server.c` and other header ,codes and scripts to run the NFS.
I have provided with `build.sh` script to make the binaries and flat file for the NFS. By default the `client.c` will connect to `localhost`.If you are running the `client` & `server` binaries on different systems then you should change the IP accordingly .

**Steps to follow :**
1)Give executable permission to `build.sh` and run it .
2)Before running the `client` , make sure to run the `server` by the simple execute command :
```sh
./server
```
3) On client side you can follow the following usage manual : 
  **Usage:**  ==./client -function <flat file name> <extra arguments>== 
  
|Function|Functionality| Extra Arguements| 
|--------|-------------|-----------------|
| rs        | Reads the superblock of our flat file                      |Nil|
| cf***       | Allows user to create file in the specified directory        |<filename> <type*> <parent inode number>|
| wf        | Allows user to choose a file using inode number and then write in it |<inode number> <data**>|
| rf        | Allows user to read a file in the specified directory        |<inode number>|
| ul***       | Unlinks a file or an empty directory                       |<filename> <parent inode number>|
| laf       | Lists all files in the specified directory                   |<parent inode>|
| lu        | Returns the inode number of a file  |<filename>|

>*Type :
> - 1 : text file 
> - 2 : directory
    
>**The user is expected to not enter data of size bigger than 511 bytes. 
    
>***It is advised to give unique names to each file and directory as you may encounter unlink bugs otherwise.
    
-------------------------------------
    
