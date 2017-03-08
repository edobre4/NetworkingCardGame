// Emanuil Dobrev
// CS 450
// HW 3
// Event driven server


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>

#define MAX_CONNECTIONS 10000

struct GameSession {
  int c1fd;
  int c2fd;
  int turn;
  int c1PlayedCard;
  int c2PlayedCard;
};

// check if card is in hand
int isCardInHand(char *hand, int n, int card) {
     int i;

     for( i = 1; i < n; i++) {
         if (hand[i] == card )
	      return 1;
     }

     return 0;
}

// function that distributes a deck of cards in 2 hands
// hand1[0] and hand2[0] indicates the request, which is 1 for start game
// hand1[1] through hand1[26] are numbers that map to a specific card
// card 0 is 2 of clubs, 2 is 3 of clubs, 13 is 2 of diamonds, 26 is 2 of hearts
// 39 is 2 of spades
void distributeCards(char *hand1, char *hand2) {

    // seed rng
    srand(time(NULL));

    // set the request byte
    hand1[0] = 1;
    hand2[0] = 1;

    // set the payload
    int cardsInHand1 = 1;
    int cardsInHand2 = 1;

    while (cardsInHand1 < 27) {
         int card = rand() % 52;
         if (! isCardInHand(hand1, cardsInHand1, card) ) {
              hand1[cardsInHand1++] = card;
         }
    }

    while (cardsInHand2 < 27) {
         int card = rand() % 52;
	 if ( !isCardInHand(hand1, cardsInHand1, card) &&
              !isCardInHand(hand2, cardsInHand2, card) ) {
              hand2[cardsInHand2++] = card;
         }
    }
}
// error - print message and exit with code -1
void error(char *msg) {
  printf("%s", msg);
  exit(-1);
}

void endGameSession(struct GameSession **gamePtr, int *clientfds, int num) {
  printf("Ending game\n");
  struct GameSession *game = *gamePtr;
  int i;
    for(i=0; i<num; i++) {
      if(clientfds[i] == game->c1fd || clientfds[i] == game->c2fd)
        clientfds[i] = -1;
    }
    close(game->c1fd);
    close(game->c2fd);  
    (*gamePtr) = (struct GameSession *) NULL;
}

int checkEndGame(struct GameSession **game, int *clientfds, int num) {
  if ((*game)->turn > 27) {
    endGameSession(game, clientfds, num);
    return 1;
  }
  return 0;
}


void sendGameResult(int pcard1, int pcard2, int sock1, int sock2, int turn, struct GameSession **game, int *clientfds, int clientNum) {
  
  int card1 = pcard1 % 13;
  int card2 = pcard2 % 13;
  int cl1_win;
  int cl2_win;
  char buffer[1025];
  int n;

  if (card1 == card2) {
    cl1_win = 2;
    cl2_win = 2;
  }
  else if (card1 > card2) {
    cl1_win = 0;
    cl2_win = 1;
  }
  else {
     cl1_win = 1; 
     cl2_win = 0;
  }
  buffer[0] = 3;
  buffer[1] = cl1_win;
          // send the result packet to each client
          n = send(sock1, buffer, 2, 0);
          printf("Round %d (%d, %d) : %d\n", turn, pcard1, pcard2, cl1_win);
          printf("sent result packet to both clients\n");
          if (n < 0)
            endGameSession(game, clientfds, clientNum);
          else {
            buffer[1] = cl2_win;
            n = send(sock2, buffer, 2, 0);
            if (n < 0)
              endGameSession(game, clientfds, clientNum);
          }
}

void playCard(struct GameSession **gamePtr, int player, int card, int *clientfds, int clientNum) {
  struct GameSession *game = *gamePtr;
  if (player == 1) {
    if (game->c1PlayedCard == -1) {
      game->c1PlayedCard = card;

      // check if other player has played his card
      if (game->c2PlayedCard != -1) {
        // calculate outcome and send result
        sendGameResult(game->c1PlayedCard, game->c2PlayedCard, game->c1fd, game->c2fd, game->turn, gamePtr, clientfds, clientNum);
        game->turn = game->turn + 1;
        game->c1PlayedCard = -1;
        game->c2PlayedCard = -1;
      }
    }
    else
      printf("player 1 attempted to play a second card during the same turn\n");
  }
  else {
    if (game->c2PlayedCard == -1) {
      game->c2PlayedCard = card;

      if (game->c1PlayedCard != -1) {
        sendGameResult(game->c1PlayedCard, game->c2PlayedCard, game->c1fd, game->c2fd, game->turn, gamePtr, clientfds, clientNum);
        game->turn = game->turn + 1;
        game->c1PlayedCard = -1;
        game->c2PlayedCard = -1;
      }
    }
    else {
      printf("player 2 attempted to play a second card during the same turn\n");
    }
  }
}

void matchPlayers(int *queue, int num, struct GameSession **game, int *clientfds, int numClients) {
  int i;
  int n = -1; int n2 = -1;

  for (i = 0; i < num; i++) {
    if (queue[i] != -1) {
      if (n == -1)
        n = queue[i];
      else
        n2 = queue[i];
    }
  }

  if (n2 == -1) {
    game = NULL;
    return;
  }
  printf("matched 2 players starting a new game\n");
  (*game) = (struct GameSession *) malloc( sizeof( struct GameSession ));
  (*game)->c1fd = n;
  (*game)->c2fd = n2;
  (*game)->turn = 1;
  (*game)->c1PlayedCard = -1;
  (*game)->c2PlayedCard = -1;
  // start game by distributing cards
     char hand1[27];
     char hand2[27];

     distributeCards(hand1, hand2);
     // remove players from queue
     for(i = 0; i < num; i++) {
       if (queue[i] == n || queue[i] == n2)
         queue[i] = -1;
     }
     // send the start game packet     
     i = send(n, hand1, 27, 0);
     if (i < 0)
          endGameSession(game, clientfds, numClients);
     else {
       i = send(n2, hand2, 27, 0);
       if (i < 0)
            endGameSession(game, clientfds, numClients);
       }
}

// function get_serverfd()
// takes a port and returns a socket file descriptor, that has already succesfully called bind()
// if there is an error in binding, the program exits.
// supports both IPv4 and IPv6, and uses TCP sockets
int get_serverfd(char *port) {
  int fd, s;

  // used for getaddrinfo() to support both IPv4 and IPv6
  struct addrinfo hints;
  struct addrinfo *result, *rp; 
 
  // clear the fields of hints
  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  s = getaddrinfo(NULL, port, &hints, &result);

  // check for error
  if (s != 0) 
    error("error getting address info\n");

  // try each address until we successfully bind
  for( rp=result; rp != NULL; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0)
      continue;
    if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;  // success
    close(fd);
  }
  
  // no address succeeded
  if(rp == NULL) 
    error("Could not bind\n");
  freeaddrinfo(result);

  // bind successful, return the fd
  return fd;
}


// main function
int main(int argc, char **argv) {
  int serverfd;  // socket fd for server
  int clientfds[MAX_CONNECTIONS]; // an array sockets fds for clients
  struct GameSession **games = (struct GameSession **) malloc( (MAX_CONNECTIONS / 2) * sizeof(struct GameSession *));

  int k ;
  int maxfd;
  for (k = 0; k < (MAX_CONNECTIONS / 2); k++) {
    games[k] = NULL;
  }

  int gameNum = 0;
  int playersInQueue[MAX_CONNECTIONS];
  int numPlayers = 0;

  int i;
  for (i = 0; i < MAX_CONNECTIONS; i++)
    playersInQueue[i] = -1;

  for( i=0; i< MAX_CONNECTIONS; i++)
    clientfds[i] = -1;

  int ncc = 0;    // number of client connections
  char buffer[1024];
  // client addr structure
  struct sockaddr_in cliaddr;
  socklen_t clilen;
  clilen = sizeof(cliaddr);

  // read fd structure used for select
  fd_set readfds;

  // program requires 1 argument - the port the server will listen on
  if (argc != 2) {
    error("Usage: server portNumber\n");
  }

  // get the server socket fd, and mark it as a passive socket
  serverfd = get_serverfd(argv[1]);
  if ( listen(serverfd, MAX_CONNECTIONS) < 0 )
    error("listen error\n");
  maxfd = serverfd + 1;
 
  // infinite loop
  while(1) {
    FD_ZERO(&readfds);
    FD_SET(serverfd, &readfds);
    int j;
    for (j = 0; j < ncc; j++) {
      if(clientfds[j] >= 0)
        FD_SET(clientfds[j], &readfds);
    }
    
    int res = select(maxfd, &readfds, NULL, NULL, NULL);
    if ((res < 0)) 
    {
      error("select error");
    }

    if (FD_ISSET(serverfd, &readfds) ) {
      clientfds[ncc] = accept(serverfd, (struct sockaddr *) &cliaddr, &clilen);
      printf("client fd %d connected\n", clientfds[ncc]);
      if(clientfds[ncc] > serverfd)
        maxfd = clientfds[ncc] + 1;
      ncc++;
    }
    else {
      // check if client has received a packet
      int j;
      for(j = 0; j < ncc; j++) {
        if (clientfds[j] < 0)
          continue;
        if ( FD_ISSET(clientfds[j], &readfds )) {
          if (recv(clientfds[j], buffer,1024, 0) <= 0 ) {
            printf("receive error\n");
            // get the game session and end it
            int f; 

            // check if player is in queue
            for (f=0; f < numPlayers;f++) {
              if(playersInQueue[f] == clientfds[j])
                playersInQueue[f] = -1;
            }
 
            for (f=0; f < gameNum; f++) {
              if(games[gameNum] == NULL)
                continue;
              if(games[f]->c1fd == clientfds[j] || games[f]->c2fd == clientfds[j]) {
                endGameSession(&(games[f]), clientfds, ncc); 
              }
            }
            clientfds[j] = -1;
          }
  
          // check if this is a want game request
          if (buffer[0] == 0) {
            printf("received a want game request from client %d\n", clientfds[j]);
            playersInQueue[numPlayers] = clientfds[j];
            // match 2 players in a game
            matchPlayers(playersInQueue, ++numPlayers, &(games[gameNum]), clientfds, ncc);
            if( games[gameNum] != NULL )
              gameNum++;
          } // if buff
  
          // check if this is a play card request
          if (buffer[0] == 2) {
            printf("received a play card request from client %d\n", clientfds[j]);
            // check which game session this client is part of
            int k; 
            for (k=0; k < gameNum; k++) {
              if(games[k] == NULL)
                continue;
              if (games[k]->c1fd == clientfds[j] ) {
                playCard(&games[k], 1, buffer[1], clientfds, ncc);
                checkEndGame(&games[k], clientfds, ncc);
                break;
              }
              if (games[k]->c2fd == clientfds[j] ) {
                playCard(&games[k], 2, buffer[1], clientfds, ncc); 
                checkEndGame(&games[k], clientfds, ncc);
                break;
              } 
              if(k == (gameNum-1) )
                printf("client %d that was not in a game session attempted to play card\n", clientfds[j]);
            }
          }
          

        } //if fd_isset
      } // for j=0
    } // else
  }
  return 0;
}
