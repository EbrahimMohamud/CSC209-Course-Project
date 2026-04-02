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
#define MAX_PLAYERS 5
#define SERVER_COUNTDOWN 200 // 200 frames in total so with 1 frame per second we get about 3 minutes of game time (Contact me for more info about fps decision) 
//End: Constant Definitions


// Start: Player Structure Definition
typedef struct {
    int socket; // Socket for communicating with the player
    int body_length; // Length of the player's snake body
    int player_head_x_coordinate; // X-coordinate of the player's snake head
    int player_head_y_coordinate; // Y-coordinate of the player's snake head
    char direction; // Current direction of the player's snake (e.g., 'U' for up, 'D' for down, 'L' for left, 'R' for right)
    int alive; // Flag to indicate if the player is alive (1) or has been eliminated (0)

} Player;
// End: Player Structure Definition


//Start: Function Blueprints
void initialize_game_map(FILE *fp, int game_map[MAP_LENGTH][MAP_WIDTH], int *rows, int *cols); // Prepare the game map according to the the user input
void initialize_spawn_map(int particle_spawn_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols); // Initialize secondary map for particle and player spawning
void spawn_particle(int particle_spawn_map[MAP_LENGTH][MAP_WIDTH], int current_game_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols, int particle_position[2]); // Spawn food particle on the game map according to the empty tiles on the spawn map and return the particle's position through the particle_position parameter
void spawn_player(int current_game_map[MAP_LENGTH][MAP_WIDTH], int spawn_map[MAP_LENGTH][MAP_WIDTH], int rows, int cols, int player_position[2]); // Spawn a new player on the game map according to the empty tiles on the spawn map and return the player's initial position through the player_position parameter
void update_both_maps(int current_game_map[MAP_LENGTH][MAP_WIDTH], int spawn_map[MAP_LENGTH][MAP_WIDTH], int x_coordinate, int y_coordinate, char update_type); // Update both the game map and the spawn map according to the update type ("p" for particle spawn, "x" for player spawn, "." for emptying a tile, "o" for snake body spawn)
int find_max_fd(Player players[], int player_count); // Find the maximum file descriptor among the player sockets for use in the select function
//End: Function Blueprints


int main(int argc, char **argv) {

    //Start: Define Game Variables
    int current_game_map[MAP_LENGTH][MAP_WIDTH]; // "." for free tile, "o" for snake body, "x" for snake head, "s" for food
    int spawn_map[MAP_LENGTH][MAP_WIDTH]; // 0 for occupied tile, 1 for free tile (used for spawning food and new players)
    int rows = 0;
    int cols = 0;
    int countdown = SERVER_COUNTDOWN; // Countdown for the game loop, decrements every frame and when it reaches 0 the game ends

    Player players[MAX_PLAYERS]; // Array to store player information
    int player_count = 0;
    
    int spawn_position[2] = {0, 0}; // Array to store the initial position of a new spawn (player or food particle), spawn_position[0] is the x-coordinate and spawn_position[1] is the y-coordinate

    FILE *fp = NULL;
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

    //Start: Initialize File Descriptor Set for Player Action Requests
    fd_set player_move_fds;
    FD_ZERO(&player_move_fds);
    //End: Initialize File Descriptor Set for Player Action Requests

    //Start: Initialize Time Val For Select Function
    struct timeval timeout;
    timeout.tv_sec = 0; // Set the timeout for the select function to 1
    timeout.tv_usec = 0; // Set the microseconds part of the timeout to 0
    //End: Initialize Time Val For Select Function

    
    //Start: Handle Server Manager Input
    if(argc == 1) {
        fp = stdin; // Read the game map features from stdin
    } else if (argc == 2) {
        fp = fopen(argv[1], "r"); // Read the game map features from the file named in argv[1]
        if(fp == NULL) {
            fprintf(stderr, "Error: could not open %s\n", argv[1]);
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: Slither.io Usage: ./executable_program_name [board_file]\n");
        exit(1);
    }
    //End: Handle Server Manager Input


    //Start: Prepare Game Setup
    initialize_game_map(fp, current_game_map, &rows, &cols);
    initialize_spawn_map(spawn_map, rows, cols);
    //End: Prepare Game Setup

    
    //Start Game Loop
    while (countdown != 0){


        //Start: Handle Player Connections
        if (player_count < MAX_PLAYERS){

                //Start: Create File Descriptor Set for Player Join Requests
                fd_set join_request_fds;
                FD_ZERO(&join_request_fds);
                FD_SET(listen_socket, &join_request_fds); // Add the listening socket to the file descriptor set so we can monitor player join requests
                //End: Create File Descriptor Set for Player Join Requests

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
                update_both_maps(current_game_map, spawn_map, spawn_position[0], spawn_position[1], 'x'); // Update both the game map and the spawn map to reflect the new player's position

                // Initialize the new player's information
                players[player_count].socket = new_player_socket;
                players[player_count].body_length = 1;
                players[player_count].player_head_x_coordinate = spawn_position[0]; // Set the player's initial x-coordinate to the x-coordinate of the spawn position
                players[player_count].player_head_y_coordinate = spawn_position[1]; // Set the player's initial y-coordinate to the y-coordinate of the spawn position
                players[player_count].direction = 'R'; // By default, the player's snake will start moving to the right 
                players[player_count].alive = 1; // Set the player's alive status to 1 (alive)

                player_count++; // Increment the player count

                FD_SET(new_player_socket, &player_move_fds); // Add the new player's socket to the file descriptor set for monitoring player action requests
            }
        }
        //End: Handle Player Connections

        for (int i = 0; i < 2; i++) // Spawn 2 food particles on the game map every frame
        {
            spawn_particle(spawn_map, current_game_map, rows, cols, spawn_position); // 
            update_both_maps(current_game_map, spawn_map, spawn_position[0], spawn_position[1], 'p');
        }

        
        


        






countdown--;
}

return 0;
}