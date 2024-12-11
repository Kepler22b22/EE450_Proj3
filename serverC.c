/*
  Author: Muqi Zhang
  Date: Dec 8 2024
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <netdb.h> // Required for addrinfo, getaddrinfo, freeaddrinfo
#include <errno.h> // Required for errno handling and error codes

#define MAX_LINE 1024
#define MAX_DEPARTMENTS 10
#define CAMPUS_SERVER_PORT "33575"
#define MAIN_SERVER_PORT "34575"
#define MAXBUFLEN 1024


// Structure to store room details
typedef struct Room {
    char type[2];          // Room type: S, D, or T
    int building_id;       // Building ID
    int availability;      // Available slots
    int price;             // Room price
} Room;

// Structure to store department and room information
typedef struct Department {
    char name[20];         // Department name
    Room rooms[MAX_LINE];  // Array of rooms
    int room_count;        // Number of rooms
} Department;

Department departments[MAX_DEPARTMENTS];
int department_count = 0;

// Function to parse the data file
void parseData(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];
    fgets(line, sizeof(line), file); // Read the first line (department names)
    char* token = strtok(line, ",");
    while (token) {
        strcpy(departments[department_count].name, token);
        departments[department_count].room_count = 0;
        department_count++;
        token = strtok(NULL, ",");
    }

    while (fgets(line, sizeof(line), file)) {
        Room room;
        sscanf(line, "%1s,%d,%d,%d", room.type, &room.building_id, &room.availability, &room.price);
        for (int i = 0; i < department_count; i++) {
            departments[i].rooms[departments[i].room_count++] = room;
        }
    }

    fclose(file);
}

// Function to send department list to the Main Server
/*void sendDepartmentList(const char* server_ip) {
    int sockfd;
    struct addrinfo server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
//    server_addr.sin_port = htons(CAMPUS_SERVER_PORT);
    server_addr.ai_socktype = SOCK_DGRAM;
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // Create the department list message
    char message[MAX_LINE] = {0};
    for (int i = 0; i < department_count; i++) {
        strcat(message, departments[i].name);
        if (i < department_count - 1) {
            strcat(message, ",");
        }
    }

    // Send the message
    sendto(sockfd, message, strlen(message), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
    printf("Server C has sent a department list to Main Server.\n");

    close(sockfd);
}*/
void sendDepartmentList(const char *main_server_ip) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP socket

    if ((rv = getaddrinfo(main_server_ip, MAIN_SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Socket creation failed");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to create socket\n");
        freeaddrinfo(servinfo);
        exit(1);
    }

    // Create the department list message
    char message[MAX_LINE] = "C"; // Start with server identifier 'C'
    for (int i = 0; i < department_count; i++) {
        strcat(message, ",");
        strcat(message, departments[i].name);
    }

    // Send the message
    if (sendto(sockfd, message, strlen(message), 0, p->ai_addr, p->ai_addrlen) == -1) {
        perror("sendto failed");
        close(sockfd);
        freeaddrinfo(servinfo);
        exit(1);
    }

    printf("Server C has sent the department list to the Main Server\n");

    close(sockfd);
    freeaddrinfo(servinfo);
}


// Helper function to handle queries
void handleAvailability(const char* room_type, char* response) {
    int total_available = 0;
    char available_buildings[MAX_LINE] = {0};

    for (int i = 0; i < department_count; i++) {
        for (int j = 0; j < departments[i].room_count; j++) {
            Room room = departments[i].rooms[j];
            if (strcmp(room.type, room_type) == 0 && room.availability > 0) {
                total_available += room.availability;
                char temp[50];
                sprintf(temp, "%d,", room.building_id);
                strcat(available_buildings, temp);
            }
        }
    }

    if (total_available > 0) {
        snprintf(response, MAX_LINE, "Server C found %d available rooms for %s type dormitories in Buildings: %s",
                 total_available, room_type, available_buildings);
    } else {
        snprintf(response, MAX_LINE, "Room type %s is not available in Server C.", room_type);
    }
}

// Helper function to sort rooms by price
void sortRoomsByPrice(Room* rooms, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (rooms[j].price > rooms[j + 1].price) {
                Room temp = rooms[j];
                rooms[j] = rooms[j + 1];
                rooms[j + 1] = temp;
            }
        }
    }
}

// Function to handle price queries
void handlePrice(const char* room_type, char* response) {
    Room available_rooms[MAX_LINE];
    int room_count = 0;

    for (int i = 0; i < department_count; i++) {
        for (int j = 0; j < departments[i].room_count; j++) {
            Room room = departments[i].rooms[j];
            if (strcmp(room.type, room_type) == 0 && room.availability >= 0) {
                available_rooms[room_count++] = room;
            }
        }
    }

    if (room_count > 0) {
        sortRoomsByPrice(available_rooms, room_count);
        snprintf(response, MAX_LINE, "Server C found room type %s with prices:\n", room_type);
        for (int i = 0; i < room_count; i++) {
            char temp[50];
            sprintf(temp, "Building ID %d, Price $%d\n", available_rooms[i].building_id, available_rooms[i].price);
            strcat(response, temp);
        }
    } else {
        snprintf(response, MAX_LINE, "Room type %s is not available in Server C.", room_type);
    }
}

// Function to handle reservation queries
void handleReservation(const char* room_type, int building_id, char* response) {
    bool room_found = false;
    bool building_found = false;

    for (int i = 0; i < department_count; i++) {
        for (int j = 0; j < departments[i].room_count; j++) {
            Room* room = &departments[i].rooms[j];
            if (strcmp(room->type, room_type) == 0) {
                room_found = true;
                if (room->building_id == building_id) {
                    building_found = true;
                    if (room->availability > 0) {
                        room->availability--;
                        snprintf(response, MAX_LINE,
                                 "Server C found room type %s in Building ID %d.\nThis room is reserved, and availability is updated to %d.",
                                 room_type, building_id, room->availability);
                    } else {
                        snprintf(response, MAX_LINE,
                                 "Server C found room type %s in Building ID %d.\nThis room is not available.",
                                 room_type, building_id);
                    }
                    return; // Exit once the reservation is processed
                }
            }
        }
    }

    if (!room_found) {
        snprintf(response, MAX_LINE, "Room type %s does not show up in Server C.", room_type);
    } else if (!building_found) {
        snprintf(response, MAX_LINE, "Building ID %d does not show up in Server C.", building_id);
    }
}

// Function to handle queries from Main Server
void handleQueries(const char* server_ip) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    char buf[MAXBUFLEN];
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP socket
    hints.ai_flags = AI_PASSIVE;     // Use my IP

    if (getaddrinfo(NULL, CAMPUS_SERVER_PORT, &hints, &servinfo) != 0) {
        perror("getaddrinfo failed");
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Socket creation failed");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Bind failed");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to bind socket\n");
        freeaddrinfo(servinfo);
        exit(1);
    }

    freeaddrinfo(servinfo);
 
    printf("Server C is up and running using UDP on port %s.\n", CAMPUS_SERVER_PORT);

    // Wait for wake-up message from the main server
    memset(buf, 0, sizeof(buf));
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom failed");
        close(sockfd);
        exit(1);
    }

    buf[numbytes] = '\0';
    if (strcmp(buf, "wake-up") == 0) {
        sendDepartmentList(server_ip);
    }

    while (1) {
        memset(buf, 0, sizeof(buf));
        if ((numbytes = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom failed");
            continue; // Do not exit; continue receiving queries
        }

        char query_type[10], room_type[2];
        int building_id;
        sscanf(buf, "%s %s %d", query_type, room_type, &building_id);

        char response[MAX_LINE] = {0};
        if (strcmp(query_type, "availability") == 0) {
            printf("Server C has received a query of Availability for room type %s.\n", room_type);
            handleAvailability(room_type, response);
        } else if (strcmp(query_type, "price") == 0) {
            printf("Server C has received a query of Price for room type %s.\n", room_type);
        } else if (strcmp(query_type, "reserve") == 0) {
            printf("Server C has received a query of Reserve for room type %s at Building ID %d.\n", room_type, building_id);
        }

        if (sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&their_addr, addr_len) == -1) {
            perror("sendto failed");
        }
    }

    close(sockfd);
}

// Main function
int main() {
    const char* filename = "dataC.txt";
    const char* server_ip = "127.0.0.1";

    parseData(filename);
    handleQueries(server_ip);

    return 0;
}
