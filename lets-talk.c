/*
 * Filename: lets-talk.c
 * Description: Chatbot
 * Date: June 25, 2022
 * Name: Daven Chohan
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>  
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "list.h"

#define INPUT_BUFFER_SIZE 100
#define UDP_BUFFER_SIZE 64000 //64000 is the max size UDP can send

int finished = 0; //0 is false 1 is true, determines if the chat is finished
int dont_read = 0; //Exit command was received or sent

int sockfd;
struct sockaddr_in receiver_addr, sender_addr;
struct sockaddr_storage storage;
pthread_t sender, receiver, input, output;

struct addrinfo *servinfo, hints, *p;

List* sent_list;
List* receive_list;

pthread_mutex_t sent_mutex, receive_mutex;
pthread_cond_t can_send, can_read, can_print, can_receive;

char received_message[UDP_BUFFER_SIZE];
char* user_input;

int other_user_status = 0; //Updates if the other user is online
int status_update = 0; //Updates that this user is online


//Function Declarations

void* read_input(void* ptr);
void* print_msg(void* ptr);
void* encrypt(char* word);
void* decrypt(char* word);
void* end_connection();
void* read_input(void* ptr);
void* print_msg(void* ptr);
void* start_sending(void* args);
void* start_receiving(void* args);
void * initialize_sender(char* address, char* port);
void * initialize_receiver(int port);

//Encrypt the sent message
void* encrypt(char* word){
    for (int i = 0; i < strlen(word); ++i)
    {
        //if (isalpha(word[i]) != 0)
        //{
         //   word[i] = word[i]+ (27 % 256);
        //}
        word[i]++;
    }
    return 0;
}

//Decrypt the received message
void* decrypt(char* word){
    for (int i = 0; i < strlen(word); ++i)
    {
        //if (isalpha(word[i]) != 0)
        //{
        //    word[i] = word[i]- (27 % 256);
        //}
        word[i]--;
    }
    return 0;
}

//End the connection
void* end_connection(){
    //free(received_message);
    free(user_input);
    freeaddrinfo(servinfo);
    finished = 1;
    List_free(sent_list, NULL);
    List_free(receive_list, NULL);
    int close(int sockfd);
    fflush(stdout);
    //exit(EXIT_SUCCESS);
}

//Read the user input
void* read_input(void* ptr){
    user_input = NULL;
    while(!finished && !dont_read){
        usleep(10000); //Used because then copy and pasted lines get sent out of order because they send too quickly
        char buffer[INPUT_BUFFER_SIZE];
        int bufferlength = 0;
        int inputlength = 0;
        if (List_count(sent_list) < 1)
        {
            do {
                fgets(buffer, INPUT_BUFFER_SIZE, stdin); // get buffer amount of characters characters
                bufferlength = strlen(buffer);
                if (!user_input)
                {
                    user_input = malloc(bufferlength+1); //malloc memory for first buffer amount of char
                }
                else {
                    user_input = realloc(user_input, bufferlength+inputlength+1); //realloc memory for next buffer amount of char
                }
                strcpy(user_input+inputlength, buffer); //copy the chars back to input
                inputlength += bufferlength;
            } while(bufferlength == INPUT_BUFFER_SIZE - 1); //while there are still characters left
            status_update = 1;
            if (strcmp(user_input, "!exit\n") == 0)
            {
                dont_read = 1;
            }
            pthread_mutex_lock(&sent_mutex);
            if (List_count(sent_list) > LIST_MAX_NUM_NODES)
            {
                pthread_cond_wait(&can_read, &sent_mutex);
            }
            List_append(sent_list, user_input);
            pthread_cond_signal(&can_send);
            pthread_mutex_unlock(&sent_mutex);
        }
    }
    return 0;
}

//Print the received messages
void* print_msg(void* ptr) {
    while(!finished){
        char* message;
        pthread_mutex_lock(&receive_mutex);
        if (List_count(receive_list) < 1)
        {
            pthread_cond_wait(&can_print, &receive_mutex);
        }
        message = List_trim(receive_list);
        if (message != NULL)
        {
            if (strcmp(message, "!status\n") != 0 && (strcmp(message, "\n") != 0)) //Status will only print locally
            {
                printf("%s", message);
            }
            else{
                pthread_mutex_lock(&sent_mutex);
                char status_command[] = ""; //Reply back to a status call to let the other user know you're online
                List_append(sent_list, status_command);
                pthread_cond_signal(&can_send);
                pthread_mutex_unlock(&sent_mutex);
            }
            if (strcmp(message, "!exit\n") == 0)
            {
                end_connection();
                pthread_cond_signal(&can_send);
                pthread_cond_signal(&can_read);
                pthread_cond_signal(&can_receive);
                break;
            }
            memset(&received_message, 0, UDP_BUFFER_SIZE);
            fflush(stdout);
            pthread_cond_signal(&can_receive);
            pthread_mutex_unlock(&receive_mutex);
        }
    }
}

//Start sending any messages inputted by the user
void* start_sending(void* args){
    char exit_command[] = "!exit\n";
    char status_command[] = "!status\n";
    int len = strlen(status_command);
    encrypt(status_command);
    int send = sendto(sockfd, status_command, len, 0, p->ai_addr, p->ai_addrlen); //used to tell other server that its online initially
    if (send < 0)
    {
        perror("Sendto error");
        exit(EXIT_FAILURE);
    }
    encrypt(exit_command);
    decrypt(status_command);
    while(!finished){
        char* sent_message;
        pthread_mutex_lock(&sent_mutex);
        if (List_count(sent_list) < 1)
        {
            pthread_cond_wait(&can_send, &sent_mutex);
        }
        if (List_count(sent_list) > 0)
        {
            sent_message = List_trim(sent_list);
            encrypt(sent_message);
            if (other_user_status != 0)
            {
                len = strlen(sent_message);
                send = sendto(sockfd, sent_message, len, 0, p->ai_addr, p->ai_addrlen);
                //printf("%s\n", "sent the message");
                if (send < 0)
                {
                    perror("Sendto error");
                    exit(EXIT_FAILURE);
                }
            }
            if (strcmp(sent_message, exit_command) == 0)
            {
                end_connection();
                pthread_cond_signal(&can_print);
                pthread_mutex_unlock(&sent_mutex);
                break;
            }
            decrypt(sent_message);
            //printf("%s%s\n", "The sent message is: ", sent_message);
            if ((strcmp(sent_message, status_command) == 0) && (status_update == 1))
            {
                if (other_user_status == 0)
                {
                    char* status = "Offline\n";
                    List_append(receive_list, status);
                    pthread_cond_signal(&can_print);
                    //printf("%s\n", status);
                } else {
                    char* status = "Online\n";
                    List_append(receive_list, status);
                    pthread_cond_signal(&can_print);
                    //printf("%s\n", status);
                }
            }
        }
        pthread_cond_signal(&can_read);
        pthread_mutex_unlock(&sent_mutex);
    }
}

//Start receiving messages from the other user
void* start_receiving(void* args){
     while(!finished && !dont_read){
        //received_message = malloc(sizeof(char*)*UDP_BUFFER_SIZE);
        socklen_t addr_len = sizeof(struct sockaddr_in);
        int receive = recvfrom(sockfd, received_message, sizeof(received_message), 0, (struct sockaddr*)&receiver_addr, &addr_len);
        //printf("%s\n", "received the message");
        if (receive < 0)
        {
            perror("Recvfrom error");
            exit(EXIT_FAILURE);
        }
        other_user_status = 1; //Other user is now online
        decrypt(received_message);
        if (strcmp(received_message, "!exit\n") == 0)
        {
            dont_read = 1;
        }
        pthread_mutex_lock(&receive_mutex);
        //printf("%s%s%s\n", "The received message is: ", received_message, "end of message ||||||");
        if (List_count(receive_list) > LIST_MAX_NUM_NODES)
        {
            pthread_cond_wait(&can_receive, &receive_mutex);
        }
        List_append(receive_list, received_message);
        pthread_cond_signal(&can_print);
        pthread_mutex_unlock(&receive_mutex);
        //free(received_message);
    }
}

//Create the sender port
//Partly learned from http://beej.us/guide/bgnet/ on how to get the address from a valid host name.
//Partly learned from https://linux.die.net/man/3/getaddrinfo
void * initialize_sender(char* address, char* port) {
    //memset(&sender_addr, 0, sizeof sender_addr);
    //sender_addr.sin_family = AF_INET;
    //sender_addr.sin_port = htons(port);
    //sender_addr.sin_addr.s_addr = inet_addr(address);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    int addr = getaddrinfo(address, port, &hints, &servinfo);
    if (addr != 0)
    {
        printf("%s\n", "Usage:\n  ./lets-talk <local port> <remote host> <remote port>\nExamples:\n  ./lets-talk 3000 192.168.0.513 3001\n  ./lets-talk 3000 some-computer-name 3001");
        exit(EXIT_FAILURE);
    }
    for (p = servinfo; p != NULL; p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("initialize socket error");
            continue;
        }


        break;
    }
    if (p==NULL)
    {
        printf("%s\n", "Usage:\n  ./lets-talk <local port> <remote host> <remote port>\nExamples:\n  ./lets-talk 3000 192.168.0.513 3001\n  ./lets-talk 3000 some-computer-name 3001");
        exit(EXIT_FAILURE);
    }
    sent_list = List_create();

}

//Create the receiver port
void * initialize_receiver(int port) {

    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&receiver_addr, 0, sizeof receiver_addr);
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(port);
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int bind_attempt;
    if ( (bind_attempt = bind(sockfd, (struct sockaddr*) &receiver_addr, sizeof(receiver_addr))) < 0 ) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    receive_list = List_create();
}                                       

void main(int argc, char *argv[])
{

    if (argc == 4)
    {
        int local_port = atoi(argv[1]);
        char* sendto_port = argv[3];
        char* sendto_address = argv[2];
        pthread_mutex_init(&sent_mutex, NULL);
        pthread_mutex_init(&receive_mutex, NULL);
        pthread_cond_init(&can_send, NULL);
        pthread_cond_init(&can_read, NULL);
        pthread_cond_init(&can_print, NULL);
        pthread_cond_init(&can_receive, NULL);
        initialize_sender(sendto_address, sendto_port);
        initialize_receiver(local_port);
        printf("%s\n", "Welcome to LetS-Talk! Please type your messages now.");
        pthread_create(&receiver, NULL, start_receiving, NULL);
        pthread_create(&sender, NULL, start_sending, NULL);
        pthread_create(&input, NULL, read_input, NULL);
        pthread_create(&output, NULL, print_msg, NULL);
        while(!finished);
    } else {
        printf("%s\n", "Usage:\n  ./lets-talk <local port> <remote host> <remote port>\nExamples:\n  ./lets-talk 3000 192.168.0.513 3001\n  ./lets-talk 3000 some-computer-name 3001");
    }
    pthread_cancel(input); //Cancel because of fgets waiting for input
    pthread_cancel(receiver); //Cancel because of recvfrom waiting for message
    pthread_join(sender, NULL);
    //printf("%s\n", "ended connection 1");
    pthread_join(output, NULL);
    //printf("%s\n", "ended connection 2");
    pthread_join(input, NULL);
    //printf("%s\n", "ended connection 3");
    pthread_join(receiver, NULL);
    //printf("%s\n", "ended connection 4");
    return;
}