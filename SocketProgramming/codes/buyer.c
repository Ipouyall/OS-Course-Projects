#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define MAX_BUF 1024
#define MAX_PRODUCT 128

#define ALARM_TIME 60

fd_set master_set;

struct Product{
    char _name[MAX_BUF/2];
    char _seller[MAX_BUF/2];
    int _port;
    int _available;
};
typedef struct Product Product;
Product products[MAX_PRODUCT];
int product_count = 0;

void block_product(const char* p_name, int port, const char* seller){
    for(int i = 0; i < product_count; i++){
        if(strcmp(products[i]._name, p_name) == 0){
            products[i]._available = 0;
            write(STDOUT, "Blocked ", 8);
            write(STDOUT, p_name, strlen(p_name));
            write(STDOUT, "\n", 1);
            return;
        }
    }
    products[product_count]._port = port;
    strcpy(products[product_count]._name, p_name);
    strcpy(products[product_count]._seller, seller);
    products[product_count]._available = 0;
    product_count++;
}

void delete_product(const char* name, int port, const char* seller){
    for(int i = 0; i < product_count; i++){
        if(strcmp(products[i]._name, name) == 0){
            product_count--;
            for(int j = i; j < product_count; j++){
                products[j] = products[j+1];
            }
            write(STDOUT, "Expired ", 8);
            write(STDOUT, name, strlen(name));
            write(STDOUT, "\n", 1);
            return;
        }
    }
    write(STDOUT, "unknown product soled!\n ", 23);
}

void unblock_product(const char* name, int port, const char* seller){
    for(int i = 0; i < product_count; i++){
        if(strcmp(products[i]._name, name) == 0){
            write(STDOUT, "Unblocked ", 10);
            write(STDOUT, name, strlen(name));
            write(STDOUT, "\n", 1);
            products[i]._available = 1;
            return;
        }
    }
    products[product_count]._port = port;
    strcpy(products[product_count]._name, name);
    strcpy(products[product_count]._seller, seller);
    products[product_count]._available = 1;
    product_count++;
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

void get_input(const char* prompt, char* buf, const int buf_size){
    memset(buf, 0, buf_size);
    write(STDOUT, prompt, strlen(prompt));
    ssize_t len = read(STDIN, buf, buf_size);
    buf[len-1] = 0;
}

void report_error(const char* msg){
    char buff[MAX_BUF];
    memset(buff, 0, MAX_BUF);
    sprintf(buff,"%s: %s!\n",msg, strerror(errno));
    write(STDOUT, buff, strlen(buff));
}

void print_product(int index){
    char buff[MAX_BUF];
    memset(buff, 0, MAX_BUF);
    if (products[index]._available) {
        sprintf(buff, "- [%d] %s from %s\n", index, products[index]._name, products[index]._seller);
    } else {
        sprintf(buff, "- %s from %s (unavailable)\n", products[index]._name, products[index]._seller);
    }
    write(STDOUT, buff, strlen(buff));
}

void print_products() {
    write(STDOUT, "Available products:\n", 20);
    for (int i = 0; i < product_count; i++) {
        print_product(i);
    }
}
int current_tcp_sock = -1;
Product current_product;

void withdraw_offer(int sig){
    write(STDOUT, "Connection timed out!\n", 22);
    if(current_tcp_sock == -1){
        write(STDOUT, "No connection to close!\n", 24);
        return;
    }
    int tcp_sock = setupTCP();
    if(tcp_sock != -1){
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(current_product._port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        if (connect(tcp_sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
            report_error("ERROR in connecting to server");
            return;
        }
        char buff[MAX_BUF];
        memset(buff, 0, MAX_BUF);
        sprintf(buff, "withdraw %s %d", current_product._name, tcp_sock);
        if (send(tcp_sock, buff, strlen(buff), 0) == -1) {
            report_error("ERROR in sending offer");
            return;
        }
        FD_CLR(current_tcp_sock, &master_set);
        close(tcp_sock);
        close(current_tcp_sock);
        current_tcp_sock = -1;
        write(STDOUT, "Offer withdrawn!\n", 17);
    }
}

int main(int argc, char const *argv[]) {
    int bc_socket_fd, _port;
    struct sockaddr_in bc_addr;
    char username[MAX_BUF];

    if (argc < 2) {
        write(2, "[init] ERROR: you should specify the port!\n", 43);
        exit(EXIT_FAILURE);
    }
    get_input("please enter your username: ", username, MAX_BUF);

    _port = atoi(argv[1]);
    bc_socket_fd = setupUDP();
    if (bc_socket_fd == -1) {
        report_error("ERROR in creating socket");
        exit(EXIT_FAILURE);
    }
    if (bind_UDP(bc_socket_fd, &bc_addr, _port) < 0) {
        report_error("ERROR in binding socket(UDP)");
        exit(EXIT_FAILURE);
    }

    fd_set working_set;
    FD_ZERO(&master_set);
    int max_fd = bc_socket_fd > STDIN ? bc_socket_fd : STDIN;
    FD_SET(STDIN, &master_set);
    FD_SET(bc_socket_fd, &master_set);
    current_tcp_sock = -1;

    while (1) {
        working_set = master_set;
        signal(SIGALRM, withdraw_offer);
        sigaction(SIGALRM, &(struct sigaction){.sa_handler=withdraw_offer, .sa_flags=SA_RESTART}, NULL);
        if (select(max_fd + 1, &working_set, NULL, NULL, NULL) == -1) {
            report_error("[select] ERROR");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(STDIN, &working_set)) {
            char buf[MAX_BUF];
            memset(buf, 0, MAX_BUF);
            ssize_t len = read(STDIN, buf, MAX_BUF);
            buf[len - 1] = 0;
            if (strcmp(buf, "list") == 0) {
                print_products();
                continue;
            }
            if (strcmp(buf, "exit") == 0) {
                break;
            }
            if (strcmp(buf, "clear") == 0){
                system("clear");
                continue;
            }
            if (strcmp(buf, "help") == 0) {
                write(STDOUT, "Available commands:\n", 20);
                write(STDOUT, "- list: list all products\n", 26);
                write(STDOUT, "- exit: exit the program\n", 25);
                write(STDOUT, "- clear: clear the screen\n", 26);
                write(STDOUT, "- help: show this message\n", 26);
                continue;
            }
            if (strcmp(buf, "offer") == 0) {
                if(current_tcp_sock != -1){
                    write(STDOUT, "You already have an offer!\n", 27);
                    continue;
                }
                int id;
                get_input("please enter the id of the product: ", buf, MAX_BUF);
                id = atoi(buf);
                if (id < 0 || id >= product_count) {
                    write(STDOUT, "ERROR: invalid id!\n", 19);
                    continue;
                }
                if (!products[id]._available) {
                    write(STDOUT, "ERROR: this product is unavailable!\n", 36);
                    continue;
                }
                current_tcp_sock = setupTCP();
                if (current_tcp_sock == -1) {
                    report_error("ERROR in creating socket");
                    continue;
                }
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(products[id]._port);
                addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if (connect(current_tcp_sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
                    report_error("ERROR in connecting to server");
                    current_tcp_sock = -1;
                    continue;
                }
                current_product = products[id];
                get_input("please enter the price: ", buf, MAX_BUF);
                char buff[MAX_BUF];
                memset(buff, 0, MAX_BUF);
                sprintf(buff, "%s for %s from %s", buf, products[id]._name, username);
                if (send(current_tcp_sock, buff, strlen(buff), 0) == -1) {
                    report_error("ERROR in sending offer");
                    continue;
                }
                memset(buff, 0, MAX_BUF);
                max_fd = current_tcp_sock > max_fd ? current_tcp_sock : max_fd;
                FD_SET(current_tcp_sock, &master_set);
                alarm(ALARM_TIME);
                continue;
            }
            write(STDOUT, "ERROR: invalid command! (list) or (offer)\n", 42);
            continue;
        }
        if (FD_ISSET(bc_socket_fd, &working_set)) {
            char buf[MAX_BUF], p_name[MAX_BUF], seller_name[MAX_BUF];
            int port;
            memset(buf, 0, MAX_BUF);
            memset(p_name, 0, MAX_BUF);
            memset(seller_name, 0, MAX_BUF);
            recv(bc_socket_fd, buf, MAX_BUF, 0);
            Product new_product;
            if (sscanf(buf, "%s %d %s", new_product._name, &new_product._port, new_product._seller) == 3) {
                new_product._available = 1;
                products[product_count++] = new_product;
                print_product(product_count - 1);
                continue;
            }
            if (sscanf(buf, "block %s from %d %s", p_name, &port, seller_name) == 3) {
                block_product(p_name, port, seller_name);
                continue;
            }
            if (sscanf(buf, "unblock %s from %d %s", p_name, &port, seller_name) == 3) {
                unblock_product(p_name, port, seller_name);
                continue;
            }
            if (sscanf(buf, "delete %s from %d %s", p_name, &port, seller_name) == 3) {
                delete_product(p_name, port, seller_name);
                continue;
            }
            write(STDOUT, "ERROR: invalid broadcast message!\n", 34);
            write(STDOUT, buf, strlen(buf));
            write(STDOUT, "\n", 1);
            continue;
        }
        if (current_tcp_sock!= -1 && FD_ISSET(current_tcp_sock, &working_set)) {
            alarm(0);
            char buff[MAX_BUF];
            memset(buff, 0, MAX_BUF);
            recv(current_tcp_sock, buff, MAX_BUF, 0);
            write(STDOUT, buff, strlen(buff));
            write(STDOUT, "\n", 1);
            close(current_tcp_sock);
            FD_CLR(current_tcp_sock, &master_set);
            current_tcp_sock = -1;
            continue;
        }
    }

    return 0;
}