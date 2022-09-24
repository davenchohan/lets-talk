# Lets-Talk Chatbot

Created a peer-to-peer connectionless messaging application through UDP using datagram sockets

Program is multithreaded to reduce CPU idle time and increase throughput.

Implemented semaphores and mutex locks to prevent deadlock and allow threads to work concurrently with safety.

Used a linked list data structure to organize sent and received messages.

## Format

Format is ./lets-talk [my port number] [remote/local machine IP] [remote/local port number]