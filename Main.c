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
//End: Include Libraries


//Start: Constant Definitions
#define MAP_LENGTH 100
#define MAP_WIDTH 100
#define MAX_SNAKE_LENGTH 30
#define MAX_PLAYERS 5
#define SERVER_COUNTDOWN 200 // 200 frames in total so with 1 frame per second we get about 3 minutes of game time (Contact me for more info about fps decision)
//End: Constant Definitions


// Start: Player Structure Definition
typedef struct {
    int socket; // Socket for communicating with the player
    int body_length; // Length of the player's snake body
    int player_body[MAX_SNAKE_LENGTH][2]; // The head of the snake is represented at index 0
    char direction; // Current direction of the player's snake (e.g., 'W' for up, 'S' for down, 'A' for left, 'D' for right)
    int alive; // Flag to indicate if the player is alive (1) or has been eliminated (0)
} Player;
// End: Player Structure Definition


//Start: Function Blueprints

//Ebrahim
void initialize_game_map(int game_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols); // Prepare the game map according to the the user input
void initialize_spawn_map(int particle_spawn_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols); // Initialize secondary map for particle and player spawning
int find_max_fd(Player players[], int player_count); // Find the maximum file descriptor among the player sockets for use in the select function
// + Client Side



//Saran
void spawn_particle(int particle_spawn_map[MAP_LENGTH][MAP_WIDTH], int current_game_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols, int particle_position[2]); // Spawn food particle on the game map according to the empty tiles on the spawn map and return the particle's position through the particle_position parameter
void spawn_player(int current_game_map[MAP_LENGTH][MAP_WIDTH], int spawn_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols, int player_position[2]); // Spawn a new player on the game map according to the empty tiles on the spawn map and return the player's initial position through the player_position parameter
void update_player_body(Player *player); // Update the player's body coordinates based on the player's current direction and body length

//Eren
void print_game_map(int game_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols); // Print the current state of the game map to the players


//End: Function Blueprints


int main(int argc, char **argv) {

    //Start: Define Game Variables
    int current_game_map[MAP_LENGTH][MAP_WIDTH]; // "." for free tile, "o" for snake body, "x" for snake head, "s" for food
    int spawn_map[MAP_LENGTH][MAP_WIDTH]; // 1 for occupied tile, 0 for free tile (used for spawning food and new players)
    int rows = MAP_WIDTH;
    int cols = MAP_LENGTH;
    int countdown = SERVER_COUNTDOWN; // Countdown for the game loop, decrements every frame and when it reaches 0 the game ends


    Player player1 = {-1, 0, {{0, 0}}, 'D', 0};
    Player player2 = {-1, 0, {{0, 0}}, 'D', 0};
    Player player3 = {-1, 0, {{0, 0}}, 'D', 0};
    Player player4 = {-1, 0, {{0, 0}}, 'D', 0};
    Player player5 = {-1, 0, {{0, 0}}, 'D', 0};

    Player players[MAX_PLAYERS]={player1, player2, player3, player4, player5}; // Array to store player information
    int player_count = 0;

    int spawn_position[2] = {0, 0}; // Array to store the initial position of a new spawn (player or food particle), spawn_position[0] is the x-coordinate and spawn_position[1] is the y-coordinate
    //End: Define Game Variables


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
    initialize_spawn_map(spawn_map, rows, cols);
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

                spawn_player(current_game_map, spawn_map, rows, cols, spawn_position); // Spawn the new player on the game map and get the player's initial position

                // Initialize the new player's information

                int player_to_replace = 0;
                for (int i = 0; i < MAX_PLAYERS; i++){
                    if (players[i].socket == -1){
                        player_to_replace = i; // Store the index of the player slot that we will reuse for the new player
                        break; // Break out of the loop after finding the first empty slot
                    }
                }


                players[player_to_replace].socket = new_player_socket;
                players[player_to_replace].body_length = 1;
                players[player_to_replace].player_body[0][0] = spawn_position[0]; // Set the x-coordinate of the player's snake head to the x-coordinate of the spawn position
                players[player_to_replace].player_body[0][1] = spawn_position[1]; // Set the y-coordinate of the player's snake head to the y-coordinate of the spawn position
                players[player_to_replace].direction = 'D'; // By default, the player's snake will start moving to the right
                players[player_to_replace].alive = 1; // Set the player's alive status to 1 (alive)

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
                char player_move = ' '; // Variable to store the player's move (e.g., 'W' for up, 'S' for down, 'A' for left, 'D' for right)
                if(read(players[i].socket, &player_move, sizeof(char)) > 0){ // Read the player's move from the socket
                    players[i].direction = player_move; // Update the player's direction based on the received move
                    update_player_body(&players[i]); // Update the player's body coordinates based on the new direction and current body length, this will also handle the player's movement and check for collisions with food particles and other players (if there is a collision with a food particle, the player's body length will increase by 1, if there is a collision with another player, the player will be eliminated and alive status will be set to 0 and player count will be decremented by 1)
                }
                    else{ // If there was an error reading from the socket, we can assume the player has disconnected
                    close(players[i].socket); // Close the player's socket
                    players[i].socket = -1; // Set the player's socket to -1 (invalid socket)
                    players[i].alive = 0; // Set the player's alive status to 0 (disconnected/eliminated)
                    update_player_body(&players[i]); // Update the player's body to reflect the disconnection (e.g., remove the player's snake from the game map)
                    player_count--; // Decrement the player count
                }
            }

        }
        }
        // End: Handle Player Moves


        //Start: Spawn Food
        for (int i = 0; i < 2; i++){
            spawn_particle(spawn_map, current_game_map, rows, cols, spawn_position);
        }
        //End: Spawn Food

        print_game_map(current_game_map, rows, cols); // Print the current state of the game map to the players

    countdown--;
    sleep(1);
    }
    // End: Game Loop
return 0;
}