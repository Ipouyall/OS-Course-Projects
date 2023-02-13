#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstring>
#include <random>

#define GENRES_FILE "genres.csv"
#define MAX_BUF 1024
#define READ 0
#define WRITE 1

using namespace std;

const int log_level = 0;

vector<string> split(const string& str, char delimiter) {
    vector<string> internal;
    stringstream ss(str); // Turn the string into a stream.
    string tok;

    while(getline(ss, tok, delimiter)) {
        internal.push_back(tok);
    }

    return internal;
}


void genre_aggregator_util(const string& genre, int workers_num){
    if (log_level >= 2)
        cout << "aggregator started for " << genre << " @" << workers_num << endl;
    int pipe_fd = open(genre.c_str(), O_RDONLY);
    int count=0;
    char buf[MAX_BUF];
    for (int i = 0; i < workers_num; ++i) {
        bzero(buf, MAX_BUF);
        while(read(pipe_fd, buf, MAX_BUF)==0){
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(10, 400);
            usleep(dis(gen));
        }
        if (log_level >= 3)
            cout << '[' <<genre << ']' << buf << endl;
        count += atoi(buf);
    }
    close(pipe_fd);
    cout << "Total number of books in " << genre << ": " << count << endl;
}

void aggregate_genre(const string& genre, int worker_count){
    unlink(genre.c_str());
    if(mkfifo(genre.c_str(), 0666)==-1){
        if(log_level>=2)
            cout <<"[" << genre << "]" << " pipe failed" << endl;
        perror("makefifo");
        exit(1);
    }
    genre_aggregator_util(genre, worker_count);
    unlink(genre.c_str());
}

void process_init_data(string data, string& genre, int& workers_num) {
    vector<string> split_data = split(data, ',');
    workers_num = atoi(split_data[0].c_str());
    genre = split_data[1];
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    if(log_level >= 1)
        cout << "Genre Manager starting..." << endl;

    string inp;
    cin >> inp;
    if(log_level >= 2)
        cout << "Genre Manager received: " << inp << endl;
    string genre;
    int workers_num;
    process_init_data(inp, genre, workers_num);
    aggregate_genre(genre, workers_num);

    return 0;
}
