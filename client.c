#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void sendWantGame(int fd, char *buffer) {
   // send the want game request
    int n;
    printf("before want game request\n");
    n = send(fd, buffer, 2, 0);

    if (n < 0)
         error("Error sending message");
    printf("sent a want game request\n");
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    if ( argc < 3) {
         error("hostname and port required");
    }
    char buffer[256];
    portno = atoi(argv[2]);

    // create socket and validate
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    printf("opened socket\n");

    // get server addr
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    printf("before connect\n");
    // connect to server
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    printf("connected to server\n");
    bzero(buffer, 255);

 
    printf("Enter command\n");
    char command[30];
    scanf("%s", command);
    if (strcmp(command, "start") == 0) 
        sendWantGame(sockfd, buffer);
    // wait for a start game packet
    char hand[27];
    int score = 0;

    n = recv(sockfd, hand, 27, 0);
    if (n != 27)
         error("Error receiving message");
    if (hand[0] != 1)
         error("expected a game start request, but received something else");

    int turn = 1;
    int win = 0;
    srand(time(NULL));
    // play a card at each turn
    while(turn < 28) {
          // print turn and remaining card
          // randomly choose a card
          int cardIndex = (rand() % 27);

          // check if card is already played
          while(hand[cardIndex] == -1) 
               cardIndex = (rand() % 27);

          // play this card
          int card = hand[cardIndex];
          hand[cardIndex] = -1;

          // send a play card request
          buffer[0] = 2;
          buffer[1] = card;
          printf("playing %d ", card);
          n = send(sockfd, buffer, 2, 0);
          if (n < 0)
               error("Error sending message");

          // get a result packet
          n = recv(sockfd, buffer, 2, 0);
          if (n < 0)
               error("Error receiving message");
          if (buffer[0] != 3) 
               error("expected a play result response, but received something else");
          if (buffer[1] == 0) { 
            printf("win\n");
            win++;
          }
          if (buffer[1] == 1) {
            printf("lose\n");
            win--;
          }
          turn++;
    }
    printf("Game Result: %s\n", (win > 0) ? "WIN" : ((win == 0) ? "DRAW" : "LOSE"));
    close(sockfd);
    return 0;
}
