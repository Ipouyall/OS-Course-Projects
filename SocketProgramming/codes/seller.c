#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define MAX_BUF 1024
#define MAX_PRODUCT 128
#define MAX_OFFER 16

enum status{
    ready, blocked, expired
};

typedef enum status status;

struct Product{
    char _name[MAX_BUF/2];
    status _status;
};

typedef struct Product Product;

struct Offer{
    int client_fd;
    char product_name[MAX_BUF/2];
    float price;
    char costumer[MAX_BUF/2];
};

typedef struct Offer Offer;

Product products[MAX_PRODUCT];
int product_count = 0;

int find_product(char* name){
    for(int i = 0; i < product_count; i++){
        if(strcmp(products[i]._name, name) == 0 && products[i]._status != expired){
            return i;
        }
    }
    return -1;
}

void report_error(const char* msg){
    char buff[MAX_BUF];
    memset(buff, 0, MAX_BUF);
    sprintf(buff,"%s: %s!\n",msg, strerror(errno));
    write(STDOUT, buff, strlen(buff));
}

void append_to(const char* file_name, const char* msg){
    int fd = open(file_name, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if(fd < 0){
        report_error("[append_to - open] ERROR");
        return;
    }
    write(fd, msg, strlen(msg));
    close(fd);
}

Offer offers[MAX_OFFER];
int offer_count = 0;

void print_offer(int index){
    char buff[MAX_BUF];
    memset(buff, 0, MAX_BUF);
    sprintf(buff, "- [%d] %s for $%f (from %s)\n", index, offers[index].product_name, offers[index].price, offers[index].costumer);
    write(STDOUT, buff, strlen(buff));
}

void print_offers(){
    write(STDOUT, "all offers:\n", 12);
    for (int i = 0; i < offer_count; i++) {
        print_offer(i);
    }
}

void reject_offer(int offer_index) {
    int pid = find_product(offers[offer_index].product_name);
    if (pid != -1) {
        products[pid]._status = ready;
    }
    send(offers[offer_index].client_fd, "your offer rejected.", 20, 0);
    for (int j = offer_index; j < offer_count - 1; j++) {
        offers[j] = offers[j + 1];
    }
    offer_count--;
}

void accept_offer(int index, const char* log_file) {
    char buff[MAX_BUF];
    memset(buff, 0, MAX_BUF);
    sprintf(buff, "(%s) %s for %f\n", offers[index].costumer, offers[index].product_name, offers[index].price);
    append_to(log_file, buff);
    send(offers[index].client_fd, "congratulation, your offer accepted.", 36, 0);
    int pid = find_product(offers[index].product_name);
    if (pid < 0) {
        write(STDOUT, "UNEXPECTED: pid doesn't exist\n", 30);
        return;
    }
    products[pid]._status = expired;
    for (int j = index; j < offer_count - 1; j++) {
        offers[j] = offers[j + 1];
    }
    offer_count--;
}

void delete_offer(const char* name) {
    for (int i = 0; i < offer_count; i++) {
        if (strcmp(offers[i].product_name, name) == 0) {
            int pid = find_product(offers[i].product_name);
            if (pid != -1) {
                products[pid]._status = ready;
            }
            for (int j = i; j < offer_count - 1; j++) {
                offers[j] = offers[j + 1];
            }
            offer_count--;
            break;
        }
    }
}

int find_offer_index(const char* name){
    for(int i = 0; i < offer_count; i++){
        if(strcmp(offers[i].product_name, name) == 0){
            return i;
        }
    }
    return -1;
}

int setupUDP(){
    int sock;
    int broadcast = 1, opt = 1;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        write(STDERR, "[socket] ", 9);
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1) {
        write(STDERR, "[SocketRocket - broadcast] ", 27);
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        write(STDERR, "[SocketRocket - reuse] ", 23);
        return -1;
    }

    return sock;
}

int setupTCP(){
    int sock;
    int opt = 1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        write(STDERR, "[socket] ", 9);
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        write(STDERR, "[SocketRocket - reuse] ", 23);
        return -1;
    }

    return sock;
}

int bind_UDP(int sock, struct sockaddr_in* addr, int port){
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_BROADCAST;
    if (bind(sock, (struct sockaddr *)addr, sizeof(*addr)) == -1) {
        write(STDERR, "[bind] ", 7);
        return -1;
    }
    return 0;
}

int bind_TCP(int sock, struct sockaddr_in* addr, int port){
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(sock, (struct sockaddr *)addr, sizeof(*addr)) == -1) {
        write(STDERR, "[bind] ", 7);
        return -1;
    }
    return 0;
}

int accept_client(int server) {
    int client;
    struct sockaddr_in client_address;
    int address_len = sizeof(client_address);
    client = accept(server, (struct sockaddr *)&client_address, (socklen_t*) &address_len);
    if (client == -1) {
        write(STDERR, "[accept] ", 9);
        return -1;
    }
    return client;
}

void get_input(const char* prompt, char* buf, const int buf_size){
    memset(buf, 0, buf_size);
    write(STDOUT, prompt, strlen(prompt));
    ssize_t len = read(STDIN, buf, buf_size);
    buf[len-1] = 0;
}

void print_products() {
    write(STDOUT, "your products:\n", 15);
    for (int i = 0; i < product_count; i++) {
        write(STDOUT, products[i]._name, strlen(products[i]._name));
        write(STDOUT, " ", 1);
        switch (products[i]._status) {
            case ready:
                write(STDOUT, "(ready)", 7);
                break;
            case expired:
                write(STDOUT, "(expired)", 9);
                break;
            case blocked:
                write(STDOUT, "(received offer)", 16);
                break;
        }
        write(STDOUT, "\n", 1);
    }
}

int generate_number(int min, int max){
    srand(time(NULL));
    return ((227 + (rand() % 51)*(rand()%20) + (rand() % 2045)) % (max - min + 1)) + min;
}

int main(int argc, char const *argv[]) {
    int bc_socket_fd, of_socket_fd;
    int udp_port, tcp_port;
    struct sockaddr_in bc_addr, of_addr;
    char buffer[MAX_BUF] = {0}, username[MAX_BUF] = {0};

    if (argc < 2) {
        write(2, "[init] ERROR in args: you should specify UDP udp_port!\n", 55);
        exit(EXIT_FAILURE);
    }

    udp_port = atoi(argv[1]);
    bc_socket_fd = setupUDP();
    of_socket_fd = setupTCP();
    if (bc_socket_fd < 0 || of_socket_fd < 0) {
        report_error("ERROR in creating socket");
        exit(EXIT_FAILURE);
    }
    if (bind_UDP(bc_socket_fd, &bc_addr, udp_port) < 0) {
        report_error("ERROR in binding socket(UDP)");
        exit(EXIT_FAILURE);
    }
    if (argc == 3) {
        tcp_port = atoi(argv[2]);
    } else {
        write(STDOUT, "you can also specify tcp-port using program args\n", 49);
        tcp_port = generate_number(1024, 65535);
    }
    sprintf(buffer, "TCP-port is: %d\n", tcp_port);
    write(STDOUT, buffer, strlen(buffer));
    if (bind_TCP(of_socket_fd, &of_addr, tcp_port) < 0) {
        report_error("ERROR in binding socket(TCP)");
        exit(EXIT_FAILURE);
    }

    get_input("Enter your name: ", username, MAX_BUF);
    char log_file[strlen(username) + 10];
    sprintf(log_file, "./%s_log.txt", username);

    write(STDOUT, "Server is running...\n", 21);

    fd_set working_set, master_set;
    listen(of_socket_fd, MAX_OFFER);
    FD_ZERO(&master_set);
    int max_fd = bc_socket_fd > of_socket_fd ? bc_socket_fd : of_socket_fd;
    FD_SET(STDIN, &master_set);
    FD_SET(bc_socket_fd, &master_set);
    FD_SET(of_socket_fd, &master_set);

    while (1) {
        working_set = master_set;
        if (select(max_fd + 1, &working_set, NULL, NULL, NULL) == -1) {
            report_error("[select] ERROR");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(STDIN, &working_set)) {
            memset(buffer, 0, MAX_BUF);
            char option[MAX_BUF];
            memset(option, 0, MAX_BUF);
            ssize_t len = read(STDIN, buffer, MAX_BUF), offer_id;
            buffer[len - 1] = 0;
            if (strcmp(buffer, "quit") == 0) {
                break;
            }
            if (strcmp(buffer, "clear") == 0) {
                system("clear");
                continue;
            }
            if (sscanf(buffer, "new %s", products[product_count]._name) == 1) {
                if(offer_count > 0){
                    write(STDOUT, "You can't add new product while there are offers!\n", 50);
                    continue;
                }
                if(product_count >= MAX_PRODUCT) {
                    write(STDOUT, "You can't add more products!\n", 29);
                    continue;
                }
                products[product_count++]._status = ready;
                write(STDOUT, "Product added!\n", 15);
                memset(buffer, 0, MAX_BUF);
                sprintf(buffer, "%s %d %s", products[product_count - 1]._name, tcp_port, username);
                if (sendto(bc_socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &bc_addr,
                           sizeof(bc_addr)) == -1) {
                    report_error("[sendto] ERROR in broadcasting");
                    continue;
                }
                continue;
            }
            if (sscanf(buffer, "%d %s", &offer_id, option) == 2) {
                if (strcmp(option, "accept") == 0) {
                    if (offer_id < offer_count && offer_id >= 0) {
                        write(STDOUT, "offer accepted!\n", 16);
                        int pid = find_product(offers[offer_id].product_name);
                        accept_offer(offer_id, log_file);
                        print_offers();
                        memset(buffer, 0, MAX_BUF);
                        sprintf(buffer, "delete %s from %d %s", products[pid]._name, tcp_port, username);
                        if (sendto(bc_socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &bc_addr,
                                   sizeof(bc_addr)) == -1) {
                            report_error("[sendto] ERROR in broadcasting, couldn't inform other clients");
                            continue;
                        }
                        continue;
                    }
                    write(STDOUT, "this offer doesn't exist\n", 25);
                    continue;
                }
                if (strcmp(option, "reject") == 0) {
                    if (offer_id < offer_count && offer_id >= 0) {
                        write(STDOUT, "offer rejected!\n", 16);
                        int pid = find_product(offers[offer_id].product_name);
                        reject_offer(offer_id);
                        print_offers();
                        memset(buffer, 0, MAX_BUF);
                        sprintf(buffer, "unblock %s from %d %s", products[pid]._name, tcp_port, username);
                        if (sendto(bc_socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &bc_addr,
                                   sizeof(bc_addr)) == -1) {
                            report_error("[sendto] ERROR in broadcasting, couldn't inform other clients");
                            continue;
                        }
                        continue;
                    }
                    write(STDOUT, "this offer doesn't exist\n", 25);
                    continue;
                }
                write(STDERR, "ERROR: invalid command! (offer accept/decline)\n", 45);
                continue;
            }
            if (strcmp(buffer, "list") == 0) {
                print_offers();
                continue;
            }
            if (strcmp(buffer, "list -p") == 0) {
                print_products();
                continue;
            }
            if (strcmp(buffer, "help") == 0) {
                write(STDOUT, "list: list all offers\n", 22);
                write(STDOUT, "list -p: list all products\n", 27);
                write(STDOUT, "new <product_name>: add new product\n", 36);
                write(STDOUT, "clear: clear screen\n", 20);
                write(STDOUT, "quit: quit the program\n", 23);
                write(STDOUT, "<offer_id> accept: accept offer\n", 32);
                write(STDOUT, "<offer_id> reject: reject offer\n", 32);
                continue;
            }
            write(STDERR, "ERROR: invalid command! (new product_title) or (offer_id answer) (list [-p])\n", 77);
            continue;
        }
        if (FD_ISSET(of_socket_fd, &working_set)) { // new offer
            write(STDOUT, "new costumer!\n", 14);
            int client = accept_client(of_socket_fd);
            if (client == -1) {
                report_error("ERROR in accepting client");
                continue;
            }
            Offer offer;
            memset(offer.product_name, 0, MAX_BUF / 2);
            memset(offer.costumer, 0, MAX_BUF / 2);
            offer.client_fd = client;
            ssize_t len = recv(client, buffer, MAX_BUF, 0);
            if (len == -1) {
                report_error("[recv] ERROR in receiving from client");
                continue;
            }
            if (sscanf(buffer, "%f for %s from %s", &offer.price, offer.product_name, offer.costumer) == 3) {
                // add new offer to offers if product exists
                int pid = find_product(offer.product_name);
                if (pid < 0 || products[pid]._status != ready) {
                    send(client, "you can't have offer for this product", 37, 0);
                    continue;
                }
                offers[offer_count++] = offer;
                products[pid]._status = blocked;
                memset(buffer, 0, MAX_BUF);
                sprintf(buffer, "block %s from %d %s", offer.product_name, tcp_port, username);
                if (sendto(bc_socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &bc_addr, sizeof(bc_addr)) ==
                    -1) {
                    report_error("[sendto] ERROR in broadcasting");
                    continue;
                }
                write(STDOUT, "new offer:\n", 11);
                print_offer(offer_count - 1);
                continue;
            }
            memset(offer.product_name, 0, MAX_BUF);
            if (sscanf(buffer, "withdraw %s %d", offer.product_name, &client) == 2) {
                int index = find_offer_index(offer.product_name);
                if (index == -1 || offers[index].client_fd != client) {
                    send(client, "you don't have any offer", 24, 0);
                    write(STDOUT, "unknown tried to withdraw offer\n", 32);
                    continue;
                }
                offer = offers[index];
                delete_offer(offer.product_name);
                memset(buffer, 0, MAX_BUF);
                sprintf(buffer, "offer on %s withdrawn\n", offer.product_name);
                write(STDOUT, buffer, strlen(buffer));
                close(client);
                memset(buffer, 0, MAX_BUF);
                sprintf(buffer, "unblock %s from %d %s", offer.product_name, tcp_port, username);
                if (sendto(bc_socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &bc_addr, sizeof(bc_addr)) ==
                    -1) {
                    report_error("[sendto] ERROR in broadcasting");
                    continue;
                }
                continue;
            }
            write(STDOUT, "unknown command from client\n", 28);
            write(STDOUT, buffer, len);
            write(STDOUT, "\n", 1);
            send(client, "unknown format; use (<price> for <product> from <name>) or (withdraw)", 69, 0);
            continue;
        }
    }

    close(of_socket_fd);
    close(bc_socket_fd);

    return 0;
}