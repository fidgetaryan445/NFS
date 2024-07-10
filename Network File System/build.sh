gcc mkfs.c -o mkfs
./mkfs -f hello_world.fs
gcc -o server server.c udp.c
gcc -o client client.c udp.c

