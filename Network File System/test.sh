#!/bin/bash

CLIENT="./client"
FLAT_FILE="hello_world.fs"
REQUEST_COUNT=0

echo "Reading superblock..."
$CLIENT -rs $FLAT_FILE
((REQUEST_COUNT++))

for iter in {1..50}; do
    echo "=== Iteration $iter ==="
    
    # Unique dir and file names
    dirname="dir$iter"
    filename="file$iter.txt"
    
    # Create directory in root (inode 0)
    $CLIENT -cf $FLAT_FILE $dirname 2 0
    ((REQUEST_COUNT++))

    # Get dir inode
    dir_inode=$($CLIENT -lu $FLAT_FILE $dirname | awk '{print $NF}')
    ((REQUEST_COUNT++))

    # Create file in directory
    $CLIENT -cf $FLAT_FILE $filename 1 $dir_inode
    ((REQUEST_COUNT++))

    # Get file inode
    file_inode=$($CLIENT -lu $FLAT_FILE $filename | awk '{print $NF}')
    ((REQUEST_COUNT++))

    # Write to file
    $CLIENT -wf $FLAT_FILE $file_inode "Hello World in $filename"
    ((REQUEST_COUNT++))

    # Read file
    $CLIENT -rf $FLAT_FILE $file_inode
    ((REQUEST_COUNT++))

    # List files in directory
    $CLIENT -laf $FLAT_FILE $dir_inode
    ((REQUEST_COUNT++))

    # List files in root
    $CLIENT -laf $FLAT_FILE 0
    ((REQUEST_COUNT++))

    # Occasionally unlink files and directories (every 5 iterations)
    if (( iter % 5 == 0 )); then
        $CLIENT -ul $FLAT_FILE $filename $dir_inode
        ((REQUEST_COUNT++))
        $CLIENT -ul $FLAT_FILE $dirname 0
        ((REQUEST_COUNT++))
    fi

    echo "Total client requests so far: $REQUEST_COUNT"
done

echo "Finished. Total simulated client requests: $REQUEST_COUNT"
