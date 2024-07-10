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


