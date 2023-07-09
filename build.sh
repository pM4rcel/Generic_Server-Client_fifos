#!/bin/bash
output=./out;
clientSrc=./client.c;
serverSrc=./server.c;

if [ ! -d "$output" ]; then
    mkdir $output;
fi

# Checks file existence and 
# if it does not exist, then it will be created with some default usernames
if [ ! -f "users.txt" ]; then
    printf "this.user\nJoe\nMichael\n" > $output/users.txt;
fi

gcc $clientSrc -o "$output/client" -Wall && gcc $serverSrc -o "$output/server" -Wall;

echo "Project build successfully!";

read -p "Wish to run it?[y/n]" shouldStart;

if [ $shouldStart == "y" ]; then
    ./$output/server & ./$output/client
fi
