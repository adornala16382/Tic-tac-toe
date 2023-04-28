// NOTE: must use option -pthread when compiling!
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#define QUEUE_SIZE 8
#define BUFF_SIZE 300
#define TRUE 1
#define FALSE 0
#define HOSTSIZE 100
#define PORTSIZE 10

struct node{
    char* val;
    struct node *next;
};

// data to be sent to worker threads
struct connection_data {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
    pthread_t tid;
    char* name;
};

struct game_data {
    struct connection_data *player;
    int index;
    int playerNum;
    struct connection_data *opponent;
    char host[HOSTSIZE];
    char port[PORTSIZE];
};

/* shared resource */
struct node *head;
int playersReady = 0;
int curIndex = 0;
struct connection_data *player1 = NULL;
struct connection_data *player2 = NULL;
int playerTurns[16];
int gameStatus[16];
char* playerBoards[16];

/* mutex lock */
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t players_ready_cond = PTHREAD_COND_INITIALIZER;

int nextFreeIndex(){
    for(int i=0; i<16; i++){
        if(playerBoards[i]== NULL){
            return i;
        }
    }
    return -1;
}

char* initializeBoard(){
    char* board = malloc(10);
    strcpy(board, ".........");
    return board;
}

void removeBoard(int index){
    if(playerBoards[index]!=NULL){
        free(playerBoards[index]);
        playerBoards[index] = NULL;
    }
}

void freeBoards(){
    for(int i=0; i <16; i++){
        if(playerBoards[i]!=NULL){
            free(playerBoards[i]);
        }       
    }
}

int is_int(char *str) {
    char *endptr;
    strtol(str, &endptr, 10);
    return *endptr == '\0';
}

void freeStrArr(char** strArr){
    int arrSize = atoi(strArr[0]);
    for(int i=0; i < arrSize + 1;i++){
        free(strArr[i]);
    }
    free(strArr);
}

void printStrArr(char ** strArr){
    int arrSize = atoi(strArr[0]);
    for(int i=0;i<arrSize+1;i++){
        printf("[%s] - ",strArr[i]);
    }
    printf("\n");   
}

void printPlayers(){
    struct node* curNode = head;
    while(curNode != NULL){
        printf("[%s] -> ", curNode->val);
        curNode = curNode->next;
    }
    printf("\n");
}

void freePlayers(){
    struct node* curNode = head;
    while(curNode != NULL){
        struct node* nextNode = curNode->next;
        free(curNode->val);
        free(curNode);
        curNode = nextNode;
    }
    head = NULL;
}

void removePlayer(char * name){
    if (head == NULL) {
        return; // The list is empty, so there's nothing to remove
    }
    struct node *prevNode = NULL;
    struct node *curNode = head;
    while (curNode != NULL) {
        if (strcmp(curNode->val, name) == 0) {
            if (prevNode == NULL) {
                // The first node in the list matches the name
                head = curNode->next;
            } else {
                // The node to be removed is not the first node
                prevNode->next = curNode->next;
            }

            // Free the memory allocated for the removed node
            free(curNode->val);
            free(curNode);
            return;
        }
        prevNode = curNode;
        curNode = curNode->next;
    }
}

char* findPlayer(char* name){
    struct node* curNode = head;
    while(curNode != NULL){
        if(strcmp(curNode->val, name) == 0){
            return curNode->val;
        }
        curNode = curNode->next;
    }
    return NULL;  // player not found   
}

int tryAddingPlayer(char* name){
    // player list is empty
    if(head == NULL){
        head = malloc(sizeof(struct node));
        char* nameCpy = malloc(BUFF_SIZE);
        strcpy(nameCpy, name);
        head->val = nameCpy;
        head->next = NULL;
        return TRUE;    
    }
    else{
        //printPlayers();
        struct node* curNode = head;
        while(curNode->next != NULL){
            // same name
            if(strcmp(curNode->val, name) == 0){
                return FALSE;
            }
            curNode = curNode->next;
        }
        // can't add name
        if(strcmp(curNode->val, name) == 0){
            return FALSE;
        }
        // can add name
        struct node* newNode = malloc(sizeof(struct node));
        char* nameCpy = malloc(BUFF_SIZE);
        strcpy(nameCpy, name);
        newNode->val = nameCpy;
        newNode->next = NULL;
        curNode->next = newNode;
        return TRUE;
    }

    return FALSE;
}

int checkBoardWinner(int index){
    if(playerBoards[index][0]=='X' && playerBoards[index][1]=='X' && playerBoards[index][2]=='X'){
        return 0;
    }
    if(playerBoards[index][3]=='X' && playerBoards[index][4]=='X' && playerBoards[index][5]=='X'){
        return 0;
    }
    if(playerBoards[index][6]=='X' && playerBoards[index][7]=='X' && playerBoards[index][8]=='X'){
        return 0;
    }
    if(playerBoards[index][0]=='X' && playerBoards[index][3]=='X' && playerBoards[index][6]=='X'){
        return 0;
    }
    if(playerBoards[index][1]=='X' && playerBoards[index][4]=='X' && playerBoards[index][7]=='X'){
        return 0;
    }
    if(playerBoards[index][2]=='X' && playerBoards[index][5]=='X' && playerBoards[index][8]=='X'){
        return 0;
    }
    if(playerBoards[index][0]=='X' && playerBoards[index][4]=='X' && playerBoards[index][8]=='X'){
        return 0;
    }
    if(playerBoards[index][2]=='X' && playerBoards[index][4]=='X' && playerBoards[index][6]=='X'){
        return 0;
    }
    if(playerBoards[index][0]=='O' && playerBoards[index][1]=='O' && playerBoards[index][2]=='O'){
        return 1;
    }
    if(playerBoards[index][3]=='O' && playerBoards[index][4]=='O' && playerBoards[index][5]=='O'){
        return 1;
    }
    if(playerBoards[index][6]=='O' && playerBoards[index][7]=='O' && playerBoards[index][8]=='O'){
        return 1;
    }
    if(playerBoards[index][0]=='O' && playerBoards[index][3]=='O' && playerBoards[index][6]=='O'){
        return 1;
    }
    if(playerBoards[index][1]=='O' && playerBoards[index][4]=='O' && playerBoards[index][7]=='O'){
        return 1;
    }
    if(playerBoards[index][2]=='O' && playerBoards[index][5]=='O' && playerBoards[index][8]=='O'){
        return 1;
    }
    if(playerBoards[index][2]=='O' && playerBoards[index][5]=='O' && playerBoards[index][8]=='O'){
        return 1;
    }
    if(playerBoards[index][0]=='O' && playerBoards[index][4]=='O' && playerBoards[index][8]=='O'){
        return 1;
    }
    for(int i=0;i<9;i++){
        if(playerBoards[index][i] == '.'){
            return -1;
        }
    }
    return 2;
}

volatile int active = 1;
void handler(int signum){
    active = 0;
}
// set up signal handlers for primary thread
// return a mask blocking those signals for worker threads
// FIXME should check whether any of these actually succeeded
void install_handlers(sigset_t *mask){
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigemptyset(mask);
    sigaddset(mask, SIGINT);
    sigaddset(mask, SIGTERM);
}

int open_listener(char *service, int queue_size){
    struct addrinfo hint, *info_list, *info;
    int error, sock;
    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    // obtain information for listening socket
    error = getaddrinfo(NULL, service, &hint, &info_list);
    if (error) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }
    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        // if we could not create the socket, try the next method
        if (sock == -1) continue;
        // bind socket to requested port
        error = bind(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }
        // enable listening for incoming connection requests
        error = listen(sock, queue_size);
        if (error) {
            close(sock);
            continue;
        }
    // if we got this far, we have opened the socket
    break;
    }
    freeaddrinfo(info_list);
    // info will be NULL if no method succeeded
    if (info == NULL) {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }
    return sock;
}

char** parseTokens(char* input){
    int numTokens = 0;
    int inputSize = BUFF_SIZE;
    char inputCpy[inputSize];
    strcpy(inputCpy, input);
    inputCpy[inputSize-1] = '\0';
    char* curTok = strtok(inputCpy, "|");

    while(curTok != NULL){
        numTokens += 1;
        curTok = strtok(NULL, "|");
    }

    char** parsedTokens = malloc(sizeof(char*) * (numTokens+1));
    char inputCpy2[inputSize];
    strcpy(inputCpy2, input);
    inputCpy2[inputSize-1] = '\0';
    curTok = strtok(inputCpy2, "|");
    char* numTokensStr = malloc(10);
    sprintf(numTokensStr, "%d", numTokens);
    parsedTokens[0] = numTokensStr;
    int curIndex = 1;
    while(curTok != NULL){
        char* token = malloc(BUFF_SIZE);
        strcpy(token, curTok);
        parsedTokens[curIndex] = token;
        curIndex += 1;
        curTok = strtok(NULL, "|");
    }
    //printStrArr(parsedTokens);
    return parsedTokens;
}

const char* play(struct connection_data *con, char** tokens, int numTokens){
    if(numTokens != 3){
        return "INVL|37|Invalid number of arguments for PLAY|\n";
    }
    if(tryAddingPlayer(tokens[3]) == FALSE){
        //printPlayers();
        return "INVL|27|Player name already exists|\n";
    }
    //printPlayers();
    con->name = findPlayer(tokens[3]);
    playersReady += 1;
    return "WAIT|0|\n";
}

char* move(int index, char** tokens, int numTokens){
    char* tmp = malloc(BUFF_SIZE);
    if(numTokens != 4){
        strcpy(tmp,"INVL|37|Invalid number of arguments for MOVE|\n");
        return tmp;
    }
    if(atoi(tokens[2])!=6){
        strcpy(tmp,"INVL|24|Invalid format for MOVE|\n");
        return tmp;
    }
    char tmpChr = 'O';
    if(playerTurns[index]==0){
        tmpChr = 'X';
    }
    else if(playerTurns[index]==1){
        tmpChr = 'O';
    }
    if(strlen(tokens[3])==1 && tokens[3][0] == tmpChr && isdigit(tokens[4][0]) && tokens[4][1]==',' && isdigit(tokens[4][2])){
        int x = atoi(&tokens[4][0]);
        int y = atoi(&tokens[4][2]);
        if(x >= 1 && x <= 3 && y >= 1 && y <= 3){
            int newInd = (3 * (x-1)) + (y - 1);
            if(playerBoards[index][newInd] == '.'){
                playerBoards[index][newInd] = tmpChr;
                strcpy(tmp, "MOVD|16|");
                strcat(tmp, tokens[4]);
                strcat(tmp, "|");
                strcat(tmp, playerBoards[index]);
                strcat(tmp, "|\n");
                return tmp;
            }
            else{
                strcpy(tmp,"INVL|19|Spot already taken|\n");
                return tmp;
            }
        }
        else{
            strcpy(tmp,"INVL|26|Invalid position for MOVE|\n");
            return tmp;
        }
    }
    strcpy(tmp,"INVL|24|Invalid format for MOVE|\n");
    return tmp;
}

char* rsgn(int index, struct connection_data *con, char** tokens, int numTokens){
    char* tmp = malloc(BUFF_SIZE);
    if(numTokens != 2){
        strcpy(tmp,"INVL|37|Invalid number of arguments for RSGN|\n");
        return tmp;
    }
    if(strcmp(tokens[2],"0") != 0){
        strcpy(tmp,"INVL|24|Invalid format for RSGN|\n");
        return tmp;
    }
    strcpy(tmp, "OVER|");
    int size = strlen(con->name) + 16;
    char* my_string = malloc(10);  // allocate space for the string
    sprintf(my_string, "%d", size); 
    strcat(tmp, my_string);
    strcat(tmp, "|L|");
    strcat(tmp, con->name);
    strcat(tmp, " has resigned|\n");
    free(my_string);
    return tmp;
}

char* draw(int index, char** tokens, int numTokens){
    char* tmp = malloc(BUFF_SIZE);
    if(numTokens != 3){
        strcpy(tmp,"INVL|37|Invalid number of arguments for DRAW|\n");
        return tmp;
    }
    if(strcmp(tokens[3], "S") != 0 && strcmp(tokens[3], "A") != 0 && strcmp(tokens[3], "R") != 0){
        strcpy(tmp,"INVL|37|Invalid number of arguments for DRAW|\n");
        return tmp;
    }
    strcpy(tmp,tokens[1]);
    strcat(tmp,"|");
    strcat(tmp,tokens[2]);
    strcat(tmp,"|");
    strcat(tmp,tokens[3]);
    strcat(tmp,"|\n");
    return tmp;
}

const char* startGame(char** tokens, struct connection_data *con, int bytes_read){
    int numTokens = atoi(tokens[0]);
    //printStrArr(tokens);
    if(numTokens >= 1 && strcmp(tokens[1], "PLAY") != 0){
        return "INVL|16|Invalid message|\n";
    }
    else if(numTokens != 3){
        return "INVL|28|Invalid number of arguments|\n";
    }
    else if(!is_int(tokens[2])){
        return "INVL|32|Second argument is not a number|\n";
    }
    else if(atoi(tokens[2])!=(bytes_read-8)){
        return "INVL|28|Number of bytes don't match|\n";
    }
    if(strcmp(tokens[1], "PLAY") == 0){
        return play(con, tokens, numTokens);
    }
    return "INVL|16|Invalid message|\n";
}

char* runCommand(char** tokens, struct connection_data *con, int index, int bytes_read){
    char* tmp = malloc(BUFF_SIZE);
    int numTokens = atoi(tokens[0]);
    //printStrArr(tokens);
    if(numTokens < 2 || numTokens > 4){
        strcpy(tmp,"INVL|28|Invalid number of arguments|\n");
        return tmp;
    }
    else if(!is_int(tokens[2])){
        strcpy(tmp,"INVL|32|Second argument is not a number|\n");
        return tmp;
    }
    else if(atoi(tokens[2])!=(bytes_read-8)){
        strcpy(tmp,"INVL|28|Number of bytes don't match|\n");
        return tmp;
    }
    if(strcmp(tokens[1], "MOVE") == 0){
        free(tmp);
        return move(index, tokens, numTokens);
    }
    else if(strcmp(tokens[1], "RSGN") == 0){
        free(tmp);
        return rsgn(index, con, tokens, numTokens);
    }    
    else if(strcmp(tokens[1], "DRAW") == 0){
        free(tmp);
        return draw(index, tokens, numTokens);
    }
    strcpy(tmp,"INVL|16|Invalid message|\n");
    return tmp;
}

void endGame(struct game_data* con){
    playerTurns[con->index] = (playerTurns[con->index] + 1) % 2;
    gameStatus[con->index] = -1;
    pthread_mutex_unlock(&players_mutex);
    pthread_cond_broadcast(&players_ready_cond);
}

void *worker(void *arg){
    // do some work
    struct game_data *con = arg;
    char buf[BUFF_SIZE];
    int bytes;
    struct connection_data* curPlayer = con->player;
    char start[300];
    strcpy(start, "BEGN|");
    int nameSize = strlen(con->opponent->name) + 3;
    char my_string[10];  // allocate space for the string
    sprintf(my_string, "%d", nameSize); 
    strcat(start, my_string);
    if(con->playerNum == 0){
        strcat(start, "|X ");
    }
    else{
        strcat(start, "|O ");
    }
    strcat(start, con->opponent->name);
    strcat(start,"|\n");
    send(curPlayer->fd, start, strlen(start) + 1, 0);
    while(active && gameStatus[con->index] != -1){
        pthread_mutex_unlock(&players_mutex);
        pthread_testcancel();
        if(playerTurns[con->index] == con->playerNum){
            send(curPlayer->fd,"CAN_READ\n", 10, 0);
            bytes = read(curPlayer->fd, buf, BUFF_SIZE);
            pthread_mutex_lock(&players_mutex);
            if (bytes < 0) {
                printf("[%s:%s] error reading\n", con->host, con->port);
                endGame(con);
                break;
            }
            if (bytes == 0) {
                printf("[%s:%s] got EOF\n", con->host, con->port);
                endGame(con);
                break;
            } else if (bytes == -1){
                printf("[%s:%s] terminating: %s\n", con->host, con->port, strerror(errno));
                endGame(con);
                break;
            }

            //logic
            buf[bytes-1] = '\0';
            printf("[%s:%s] read %d bytes {%s}\n", con->host, con->port, bytes, buf);
            if((bytes < 8) || (buf[0] == '|') || (buf[bytes-2] != '|')){
                send(curPlayer->fd, "INVL|20|Not a valid message|\n", 30, 0); 
                pthread_mutex_unlock(&players_mutex);
                continue;
            }
            char ** tokens = parseTokens(buf);
            char* status = runCommand(tokens, curPlayer, con->index, bytes);
            char prefix[5];
            strncpy(prefix, status, 4);
            prefix[4] = '\0';
            if(gameStatus[con->index] == 1 && (strcmp(prefix, "DRAW") == 0 && (strcmp(tokens[3],"R") != 0 && strcmp(tokens[3],"A") != 0))){
                send(curPlayer->fd, "INVL|26|Invalid response for DRAW|\n", 36, 0); 
                freeStrArr(tokens);
                free(status);
                pthread_mutex_unlock(&players_mutex);
                continue;
            }
            if(strcmp(prefix, "DRAW") == 0){
                if(gameStatus[con->index] == 1){
                    if(strcmp(tokens[3],"R") == 0){
                        gameStatus[con->index] = 0;
                        send(con->opponent->fd, status, strlen(status), 0);
                    }
                    else if(strcmp(tokens[3],"A") == 0){
                        send(curPlayer->fd,"OVER|7|D|DRAW|\n", 16, 0);
                        send(con->opponent->fd,"OVER|7|D|DRAW|\n", 16, 0);
                        freeStrArr(tokens);
                        free(status);
                        endGame(con);
                        break;
                    }
                    else{
                        send(curPlayer->fd, "INVL|26|Invalid response for DRAW|\n", 36, 0); 
                        freeStrArr(tokens);
                        free(status);
                        pthread_mutex_unlock(&players_mutex);
                        continue; 
                    }
                }
                else if(strcmp(tokens[3],"S") == 0){
                    gameStatus[con->index] = 1;
                    send(con->opponent->fd, status, strlen(status), 0);
                }
                else{
                    send(curPlayer->fd, "INVL|25|Invalid message for DRAW|\n", 35, 0); 
                    freeStrArr(tokens);
                    free(status);
                    pthread_mutex_unlock(&players_mutex);
                    continue;        
                }   
            }
            else{
                send(curPlayer->fd, status, strlen(status), 0);
            }
            freeStrArr(tokens);
            if(strcmp(prefix, "INVL") == 0){
                free(status);
                pthread_mutex_unlock(&players_mutex);
                continue;
            }
            else if(strcmp(prefix, "MOVD") == 0){
                send(con->opponent->fd, status, strlen(status), 0);
                char tmp[BUFF_SIZE];
                if(checkBoardWinner(con->index)==con->playerNum){
                    char tmp2[BUFF_SIZE];
                    strcpy(tmp, "OVER|");
                    strcpy(tmp2, "OVER|");
                    char name[200];
                    strcpy(name, curPlayer->name);
                    int size = strlen(name) + 11;
                    char my_string[10];  // allocate space for the string
                    sprintf(my_string, "%d", size);
                    strcat(tmp, my_string);
                    strcat(tmp2, my_string);
                    strcat(tmp, "|W|");
                    strcat(tmp2, "|L|");
                    strcat(tmp, name); 
                    strcat(tmp2, name);
                    strcat(tmp, " has won|\n");
                    strcat(tmp2, " has won|\n");
                    send(curPlayer->fd,tmp, strlen(tmp), 0);
                    send(con->opponent->fd,tmp2, strlen(tmp2), 0);
                    free(status);
                    endGame(con);
                    break;
                }
                else if(checkBoardWinner(con->index)==2){
                    strcpy(tmp,"OVER|7|D|DRAW|");
                    send(curPlayer->fd,tmp, strlen(tmp), 0);
                    send(con->opponent->fd,tmp, strlen(tmp), 0);
                    free(status);
                    endGame(con);
                    break;
                }
            }
            else if(strcmp(prefix, "OVER") == 0){
                char tmp[300];
                strcpy(tmp, status);
                tmp[8] = 'W';
                send(con->opponent->fd, tmp, strlen(status), 0);
                free(status);
                endGame(con);
                break;
            }
            playerTurns[con->index] = (playerTurns[con->index] + 1) % 2;
            free(status);
            pthread_mutex_unlock(&players_mutex);
            pthread_cond_broadcast(&players_ready_cond);
        }
        else{
            while(playerTurns[con->index] != con->playerNum){
                pthread_cond_wait(&players_ready_cond, &players_mutex);
            }
        }
    }
    removePlayer(con->player->name);
    removeBoard(con->index);
    close(con->player->fd);
    free(con->player);
    free(con);
    pthread_mutex_unlock(&players_mutex);
    return NULL;
}

void *read_data(void *arg){
    struct connection_data *con = arg;
    char buf[BUFF_SIZE], host[HOSTSIZE], port[PORTSIZE];
    int bytes, error;
    int game_started = FALSE;
    error = getnameinfo(
        (struct sockaddr *)&con->addr, con->addr_len,
        host, HOSTSIZE,
        port, PORTSIZE,
        NI_NUMERICSERV);
    if (error) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(error));
        strcpy(host, "??");
        strcpy(port, "??");
    }
    printf("Connection from %s:%s\n", host, port);
    send(con->fd, "CAN_READ\n", 10, 0); 
    while (active && (bytes = read(con->fd, buf, BUFF_SIZE)) > 0) {
        pthread_testcancel();
        buf[bytes-1] = '\0';
        printf("[%s:%s] read %d bytes {%s}\n", host, port, bytes, buf);
        if((bytes < 8) || (buf[0] == '|') || (buf[bytes-2] != '|')){
            printf("[%c][%c]\n", buf[0], buf[bytes-2]);
            send(con->fd, "INVL|20|Not a valid message|\n", 30, 0); 
            send(con->fd, "CAN_READ\n", 10, 0);
            continue;
        }
        char ** tokens = parseTokens(buf);
        const char* status = startGame(tokens, con, bytes);
        send(con->fd, status, strlen(status), 0);
        //create new game + new thread

        char prefix[5];
        strncpy(prefix, status, 4);
        prefix[4] = '\0';
        if(strcmp(prefix, "INVL") == 0){
            send(con->fd, "CAN_READ\n", 10, 0);
            freeStrArr(tokens);
            continue;
        }
        pthread_mutex_lock(&players_mutex);
        if (playersReady == 2) {
            player2 = con;
            curIndex = nextFreeIndex();
            playerBoards[curIndex] = initializeBoard();
            pthread_cond_broadcast(&players_ready_cond);
            pthread_t tid;
            // create and initialize a new connection_data structure for the worker()
            struct game_data *worker_con = malloc(sizeof(struct game_data));
            worker_con->player = con;
            worker_con->index = curIndex;
            worker_con->playerNum = 1;
            worker_con->opponent = player1;
            strcpy(worker_con->host, host);
            strcpy(worker_con->port, port);
            pthread_create(&tid, NULL, worker, worker_con);
            pthread_cancel(con->tid);
            con->tid = tid;
            pthread_detach(tid);
            game_started = TRUE;
            // break the loop to stop read_data from reading more input
            freeStrArr(tokens);
            pthread_mutex_unlock(&players_mutex);
            break;
        } 
        else {
            player1 = con;
            while (playersReady < 2) {
                pthread_cond_wait(&players_ready_cond, &players_mutex);
            }
            pthread_t tid;
            // create and initialize a new connection_data structure for the worker()
            struct game_data *worker_con = malloc(sizeof(struct game_data));
            worker_con->player = con;
            worker_con->index = curIndex; 
            worker_con->playerNum = 0;
            worker_con->opponent = player2;
            strcpy(worker_con->host, host);
            strcpy(worker_con->port, port);
            playerTurns[curIndex] = 0;
            gameStatus[curIndex] = 0;
            playersReady = 0;
            pthread_create(&tid, NULL, worker, worker_con);
            pthread_cancel(con->tid);
            con->tid = tid;
            pthread_detach(tid);
            game_started = TRUE;
            freeStrArr(tokens);
            pthread_mutex_unlock(&players_mutex);
            break;
        }
        freeStrArr(tokens);
        send(con->fd, "CAN_READ\n", 10, 0);
    }
    if (bytes == 0) {
        printf("[%s:%s] got EOF\n", host, port);
    } else if (bytes == -1){
        printf("[%s:%s] terminating: %s\n", host, port, strerror(errno));
    } else {
        printf("[%s:%s] terminating: %s\n", host, port, strerror(errno));
    }
    // Close the file descriptor only if the game has not started
    if (!game_started) {
        close(con->fd);
        free(con);
    }
    return NULL;
}

int main(int argc, char **argv){
    sigset_t mask;
    struct connection_data *con;
    int error;
    pthread_t tid;
    char *service = argc == 2 ? argv[1] : "15000";
    printf("[%s]\n", argv[2]);
    install_handlers(&mask);
    int listener = open_listener(service, QUEUE_SIZE);
    if (listener < 0) exit(EXIT_FAILURE);
        printf("Listening for incoming connections on %s\n", service);
    while (active) {
        pthread_testcancel();
        con = (struct connection_data *)malloc(sizeof(struct connection_data));
        con->addr_len = sizeof(struct sockaddr_storage);
        con->fd = accept(listener,
        (struct sockaddr *)&con->addr, &con->addr_len);
        if (con->fd < 0) {
            perror("accept");
            free(con);
            // TODO check for specific error conditions
            continue;
        }
        // temporarily disable signals
        // (the worker thread will inherit this mask, ensuring that SIGINT is
        // only delivered to this thread)
        error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
        if (error != 0) {
            fprintf(stderr, "sigmask: %s\n", strerror(error));
            exit(EXIT_FAILURE);
        }
        error = pthread_create(&tid, NULL, read_data, con); 
        con->tid = tid;
        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            close(con->fd);
            free(con);
            continue;
        }
        // automatically clean up child threads once they terminate
        pthread_detach(tid);
        // unblock handled signals
        error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (error != 0) {
            fprintf(stderr, "sigmask: %s\n", strerror(error));
            exit(EXIT_FAILURE);
        }
    }
    freePlayers();
    puts("Shutting down");
    close(listener);
    return EXIT_SUCCESS;
}