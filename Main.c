//Start: Include Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
//End: Include Libraries


//Start: Constant Definitions
#define MAP_LENGTH 45
#define MAP_WIDTH 110
#define MAX_SNAKE_LENGTH 30
#define MAX_PLAYERS 5
#define SERVER_COUNTDOWN 200 // 200 frames in total so with 1 frame per second we get about 3 minutes of game time (Contact me for more info about fps decision)

#define EMPTY_TILE (0)
#define SNAKE_HEAD (1)
#define SNAKE_BODY (2)
#define FOOD (-1)

#define EMPTY_TILE_CHAR (".")
#define SNAKE_HEAD_CHAR ("x")
#define SNAKE_BODY_CHAR ("o")
#define FOOD_CHAR ("s")
//End: Constant Definitions

// Start: Global Variable Definitions
int dx[4] = {0, -1, 0, 1}; 
int dy[4] = {-1, 0, 1, 0}; 
// End: Global Variable Definitions 

// Start: Player Structure Definition
typedef struct {
    int socket; // Socket for communicating with the player
    int body_length; // Length of the player's snake body
    int player_body[MAX_SNAKE_LENGTH][2]; // The head of the snake is represented at index 0
    int direction[2]; // x and y coordinate of direction snake is travelling in
    int alive; // Flag to indicate if the player is alive (1) or has been eliminated (0)
} Player;
// End: Player Structure Definition


//Start: Function Blueprints

int wrap_index(int value, int size) {
    return ((value % size) + size) % size;
}

// Initalizes an empty game map with dimensions given by the rows and columns
void initialize_game_map(int game_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols) {
	// Iterate across reach row
	for (int i = 0; i < rows; i++) {
		// Iterate across each column
		for (int j = 0; j < cols; j++) {
			// sets value to EMPTY_TILE macro
			game_map[i][j] = EMPTY_TILE;
		}
	}
}


// Find the maximum file descriptor among the player sockets for use in the select function
int find_max_fd(Player players[], int player_count){
    int max_fd = -1; // Initialize max_fd to -1 (invalid file descriptor)
    for (int i = 0; i < player_count; i++){
        if (players[i].socket > max_fd){ // If the player's socket is greater than the current max_fd
            max_fd = players[i].socket; // Update max_fd to the player's socket
        }
    }
    return max_fd; // Return the maximum file descriptor found among the player sockets
}



/*  Food particle spawned randomly on empty tile in game map.
    game_map is updated.

    Note: Assume srand(time(NULL)) is called in main.c to seed the rand() function
*/
void spawn_particle(int (*game_map)[MAP_WIDTH], int rows, int cols) {
    // 1) randomize (x,y) coordinate on board
    // repeat until (x,y) corresponds to unoccupied coordinate
    int x, y;
    do {
        x = rand() % cols;
        y = rand() % rows;
    } while (game_map[y][x] != EMPTY_TILE);

    // now (x,y) corresponds to unoccupied coordinate on game_map

    // 2) update game_map to have (x,y) occupied
    game_map[y][x] = FOOD;
}


/*  New snake spawned randomly on empty tile in game map. Direction it travels is randomized.
    game_map, player are updated.
*/
void spawn_player(int (*game_map)[MAP_WIDTH], int rows, int cols, Player *player) {
    // 1) generate random (x, y) coordinate
    // repeat until (x,y) corresponds to unoccupied coordinate
    int x, y;
    do {
        x = rand() % cols;
        y = rand() % rows;
    } while (game_map[y][x] != EMPTY_TILE);

    // now (x,y) corresponds to unoccupied coordinate on game_map

    // 2) update game_map to have (x,y) occupied
    game_map[y][x] = SNAKE_HEAD;

    // 3) update player's snake
    // set player's snake to length 1
    player->body_length = 1;    

    // reset player_body's head to (x,y)
    player->player_body[0][0] = x;
    player->player_body[0][1] = y;

    // set player's snake to be alive
    player->alive = 1;

    // randomize direction
    // generate random number from 0 to 3 corresponding to W, A, S, or D directions
    int direction = rand() % 4;
    player->direction[0] = dx[direction];
    player->direction[1] = dy[direction];
}

/*  Helper function to kill snake. 
    Change all snake head and body elements of snake to food on map. Map updated accordingly.
    Updates Player snake's body_length and alive attributes.
*/
void kill_snake_body(int game_map[MAP_LENGTH][MAP_WIDTH], Player *player, int *player_count) {
    // iterate through all snake head and body elements - convert them to food on map.
    for (int i = 0; i < player->body_length; i++) {
        
        // get (x, y) coordinate of current snake element
        int x = player->player_body[i][0];
        int y = player->player_body[i][1];

        // snake element becomes food on map
        game_map[y][x] = FOOD;
    }

    // set snake's length to 0
    player->body_length = 0;
    
    // set player's snake to be dead
    player->alive = 0;

    // Close the player's socket
    close(player->socket);

    // Set player's socket to -1 (invalid socket) to indicate disconnection/elimination
    player->socket = -1;

    // Decrement player count
    (*player_count)--;
}


/*  Helper function to shift the body of the snake in the direction it is travelling. Moves every element of the snake one unit forward.
    Map updated accordingly.
*/
void shift_snake_body(int game_map[MAP_LENGTH][MAP_WIDTH], Player *player, int rows, int cols) {
    // get head of snake
    int head[2];

    head[0] = player->player_body[0][0];
    head[1] = player->player_body[0][1];

    // calculate next coordinate for head of snake
    int next_coord[2];
    next_coord[0] = wrap_index(head[0] + player->direction[0], cols);
    next_coord[1] = wrap_index(head[1] + player->direction[1], rows);

    int prev_coord[2];
    int temp_coord[2];

    for (int i = 0; i < player->body_length; i++) {

        // updating head of the snake
        if (i == 0) {

            // 1) update current location of snake head to next coordinate calculated above
            player->player_body[i][0] = next_coord[0];
            player->player_body[i][1] = next_coord[1];

            // update current coordinate on game_map
            game_map[next_coord[1]][next_coord[0]] = SNAKE_HEAD;

            // 2) update prev_coord to where snake head used to be
            prev_coord[0] = head[0];
            prev_coord[1] = head[1];

            // delete previous coordinate on game_map
            game_map[prev_coord[1]][prev_coord[0]] = EMPTY_TILE;
            
        } 
        
        // non-head elements of the snake are moved to previous coordinate
        else {

            // store current coordinate in temp
            temp_coord[0] = player->player_body[i][0];
            temp_coord[1] = player->player_body[i][1];

            // 1) update current coordinate to where the snake used to be (prev_coord)
            player->player_body[i][0] = prev_coord[0];
            player->player_body[i][1] = prev_coord[1];

            // update new coordinate on game_map
            game_map[prev_coord[1]][prev_coord[0]] = SNAKE_BODY;

            // 2) update prev_coord for next iteration
            prev_coord[0] = temp_coord[0];
            prev_coord[1] = temp_coord[1];

            // delete previous coordinate on game_map
            game_map[prev_coord[1]][prev_coord[0]] = EMPTY_TILE;
            
        }
    }
} 


/*  Position of snake is updated.
    If next coordinate is
    - an empty tile: snake moves forward
    - food: snake moves forward & grows longer by 1
    - a snake element (head or body of a snake): snake dies and turns into food
    
    Pre-condition: direction of snake is assumed to be valid. 
        (handle case of user inputs the opposite direction of the movement of the snake to do nothing in main code.)
*/
void update_player_body(int (*game_map)[MAP_WIDTH], int rows, int cols, Player *player, int *player_count) {
    // get head of snake
    int head[2];

    head[0] = player->player_body[0][0];
    head[1] = player->player_body[0][1];

    // calculate next coordinate for head of snake
    int next_coord[2];
    next_coord[0] = wrap_index(head[0] + player->direction[0], cols);
    next_coord[1] = wrap_index(head[1] + player->direction[1], rows);

    // if next coordinate has food - snake moves forward and grows longer
    if (game_map[next_coord[1]][next_coord[0]] == FOOD) {
        if (player->body_length < MAX_SNAKE_LENGTH) {
        // 1) save location of last body element of snake in a temp coord
        int temp[2];

        // player->body_length - 1 refers to the index to access last element of snake in player_body
        temp[0] = player->player_body[player->body_length - 1][0];
        temp[1] = player->player_body[player->body_length - 1][1];

        // 2) move every element of the snake one unit forward
        shift_snake_body(game_map, player, rows, cols);
        
        // 3) next coordinate is food, so increment size of snake
        player->body_length++;

        // new body element of snake added to be where the tail of the old snake used to be
        player->player_body[player->body_length - 1][0] = temp[0];
        player->player_body[player->body_length - 1][1] = temp[1];

        // update game_map to include newly added snake-body element
        game_map[temp[1]][temp[0]] = SNAKE_BODY;
    }
        else {
            // if snake is already at max length, just move the snake forward without growing
            shift_snake_body(game_map, player, rows, cols);
        }
    }

    // if next coordinate is empty - snake moves forward
    else if (game_map[next_coord[1]][next_coord[0]] == EMPTY_TILE) {
        
        // move every element of the snake one unit forward
        shift_snake_body(game_map, player, rows, cols);

    } 
    
    // next coordinate is a snake-body or a snake-head character - snake should die
    else {

        kill_snake_body(game_map, player, player_count);
    
    }
}


/*  Prints current state of game.*/
void print_game_map(int game_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols) {
    // Iterate across reach row
    for (int i = 0; i < rows; i++) {

		// Iterate across each column
		for (int j = 0; j < cols; j++) {
			
            if (game_map[i][j] == EMPTY_TILE) {

                printf(EMPTY_TILE_CHAR);

            } else if (game_map[i][j] == SNAKE_HEAD) {

                printf(SNAKE_HEAD_CHAR);

            } else if (game_map[i][j] == SNAKE_BODY) {

                printf(SNAKE_BODY_CHAR);

            } else if (game_map[i][j] == FOOD) {

                printf(FOOD_CHAR);

            }

		}

        // print new line at the end of a row
        printf("\n");

	}
    printf("\n"); // print new line at the end of the game map for better readability
    for (int i = 0; i < cols; i++) {
    printf("-"); // print a separator after the game map for better readability
    }
    printf("\n");
}



//End: Function Blueprints


int main() {

    //Start: Define Game Variables
    int current_game_map[MAP_LENGTH][MAP_WIDTH]; // "." for free tile, "o" for snake body, "x" for snake head, "s" for food
    int rows = MAP_LENGTH;
    int cols = MAP_WIDTH;
    int countdown = SERVER_COUNTDOWN; // Countdown for the game loop, decrements every frame and when it reaches 0 the game ends


    Player player1 = {-1, 0, {{0, 0}}, {0, 0}, 0}; // Initialize player1 with default values (socket = -1 indicates no player connected)
    Player player2 = {-1, 0, {{0, 0}}, {0, 0}, 0}; // Initialize player2 with default values (socket = -1 indicates no player connected)
    Player player3 = {-1, 0, {{0, 0}}, {0, 0}, 0}; // Initialize player3 with default values (socket = -1 indicates no player connected)
    Player player4 = {-1, 0, {{0, 0}}, {0, 0}, 0}; // Initialize player4 with default values (socket = -1 indicates no player connected)
    Player player5 = {-1, 0, {{0, 0}}, {0, 0}, 0}; // Initialize player5 with default values (socket = -1 indicates no player connected)

    Player players[MAX_PLAYERS]={player1, player2, player3, player4, player5}; // Array to store player information
    int player_count = 0;
    //End: Define Game Variables

    // Start: Call srand to seed the rand() function
    srand(time(NULL));
    // End: Call srand to seed the rand() function


    //Start: Create Server Socket
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0); // Create socket for listening
    if (listen_socket == -1){
        perror("listen_socket error");
        exit(1);
    }

    struct sockaddr_in server_addr;            // Define server adress structure
    server_addr.sin_family = AF_INET;          // Set address family to IPv4
    server_addr.sin_port = htons(54321);       // Set server's port number to 54321
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Set server's IP address to accept connections from any address
    memset(server_addr.sin_zero, 0, 8);        // Zero the rest of the memory following this struct for safety purposes


    if (bind(listen_socket, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in)) == -1){  //Bind the socket to the specified port and IP address
        perror("socket_bind error");
        close(listen_socket);
        exit(1);
    }

    if (listen(listen_socket, MAX_PLAYERS) == -1){ // Listen for incoming connections, max number of connections is equal to the max player number
        perror("listen_action error");
        close(listen_socket);
        exit(1);
    }
    //End: Create Server Socket


    //Start: Initialize Time Val For Select Function
    struct timeval timeout;
    timeout.tv_sec = 0; // Set the timeout for the select function to 1
    timeout.tv_usec = 0; // Set the microseconds part of the timeout to 0
    //End: Initialize Time Val For Select Function


    //Start: Prepare Game Setup
    initialize_game_map(current_game_map, rows, cols);
    //End: Prepare Game Setup


    //Start Game Loop
    while (countdown != 0){

        //Start: Create File Descriptor Set for Player Join Requests
        fd_set join_request_fds;
        FD_ZERO(&join_request_fds);
        FD_SET(listen_socket, &join_request_fds); // Add the listening socket to the file descriptor set so we can monitor player join requests
        //End: Create File Descriptor Set for Player Join Requests

        //Start: Create File Descriptor Set for Player Action Requests
        fd_set player_move_fds;
        FD_ZERO(&player_move_fds);
        //End: Create File Descriptor Set for Player Action Requests


        //Start: Handle Player Connections
        if (player_count < MAX_PLAYERS){

            if (select(listen_socket + 1, &join_request_fds, NULL, NULL, &timeout) < 0){ // Monitor the listening socket for incoming connections
                perror("select error");
                close(listen_socket);
                exit(1);
            }

            if (FD_ISSET(listen_socket, &join_request_fds)){ // If there is an incoming connection on the listening socket
                int new_player_socket = accept(listen_socket, NULL, NULL);  // Accept the incoming connection and get a new socket for communicating with the new player
                if (new_player_socket == -1){
                    perror("accept error");
                    close(listen_socket);
                    exit(1);
                }


                int player_to_replace = 0;
                for (int i = 0; i < MAX_PLAYERS; i++){
                    if (players[i].socket == -1){
                        player_to_replace = i; // Store the index of the player slot that we will reuse for the new player
                        break; // Break out of the loop after finding the first empty slot
                    }
                }

                players[player_to_replace].socket = new_player_socket;
                spawn_player(current_game_map, rows, cols, &players[player_to_replace]); // Spawn a new player on the game map and update the player's information in the players array
                player_count++; // Increment the player count

                FD_SET(new_player_socket, &player_move_fds); // Add the new player's socket to the file descriptor set for monitoring player action requests
            }
        }
        //End: Handle Player Connections


        //Start: Add Player Sockets to File Descriptor
        for (int i = 0; i < MAX_PLAYERS; i++){
            if (players[i].socket >= 0){
                FD_SET(players[i].socket, &player_move_fds); // Add each player's socket to the file descriptor set for monitoring player action requests
        }
        }
        //End: Add Player Sockets to File Descriptor


        // Start: Handle Player Moves
        if (select(find_max_fd(players, MAX_PLAYERS) + 1, &player_move_fds, NULL, NULL, &timeout) < 0){
            perror("select error");
            exit(1);
        }

        for (int i = 0; i < MAX_PLAYERS; i++){
            if (players[i].socket >= 0){ // If the player's socket is valid (greater than 0)
            if (FD_ISSET(players[i].socket, &player_move_fds)){ // If there is an incoming action request from the player

                char player_move; // Variable to store the player's move read from the socket 

                if(read(players[i].socket, &player_move, sizeof(char)) > 0){ // Read the player's move from the socket
                    if ((player_move == 'W' && players[i].direction[1] != 1) || (player_move == 'A' && players[i].direction[0] != 1) || (player_move == 'S' && players[i].direction[1] != -1) || (player_move == 'D' && players[i].direction[0] != -1)){ // If the player's move is valid (not the opposite direction of the current movement of the snake)
                        if (player_move == 'W'){
                            players[i].direction[0] = 0;
                            players[i].direction[1] = -1;
                        } else if (player_move == 'A'){
                            players[i].direction[0] = -1;
                            players[i].direction[1] = 0;
                        } else if (player_move == 'S'){
                            players[i].direction[0] = 0;
                            players[i].direction[1] = 1;
                        } else if (player_move == 'D'){
                            players[i].direction[0] = 1;
                            players[i].direction[1] = 0;
                        }
                    }

                }
                    else{ // If there was an error reading from the socket, we can assume the player has disconnected
                    kill_snake_body(current_game_map, &players[i], &player_count); // Eliminate the player's snake from the game map and update the player's information in the players array
                }
            }
            }
        }
        // End: Handle Player Moves


        // Start: Update Game State
        for (int i = 0; i < MAX_PLAYERS; i++){
            if (players[i].socket >= 0){ // If the player's socket is valid (greater than 0)
                update_player_body(current_game_map, rows, cols, &players[i], &player_count); // Update the player's snake position based on its current direction and update the game map accordingly
            }
        }
        // End: Update Game State


        //Start: Spawn Food
        spawn_particle(current_game_map, rows, cols); // Spawn a food particle on the game map at a random empty tile
        //End: Spawn Food

        print_game_map(current_game_map, rows, cols); // Print the current state of the game map to the players

    countdown--;
    sleep(1);
    }
    // End: Game Loop
return 0;
}