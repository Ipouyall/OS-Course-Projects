#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstring>
#include <random>

#define MAX_BUF 1024
#define READ 0
#define WRITE 1

using namespace std;

const int log_level = 0;

typedef struct Genre {
    string name;
    int count;
    Genre(const string& name="", int count=0) : name(name), count(count) {}
} Genre;

vector<string> split(const string& str, char delimiter) {
    vector<string> internal;
    stringstream ss(str); // Turn the string into a stream.
    string tok;

    while(getline(ss, tok, delimiter)) {
        internal.push_back(tok);
    }

    return internal;
}


void count_genres_from_file(const string& filename, vector<Genre>& genres) {
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        vector<string> split_line = split(line, ',');
        split_line.erase(split_line.begin());
        for (auto& genre: split_line) {
            for (auto& g: genres) {
                if (g.name == genre) {
                    g.count++;
                    break;
                }
            }
        }
    }
    file.close();
}

void random_sleep(int max){
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, max);
    int sleep_time = dis(gen) + 100;
    sleep(sleep_time);
}

void report_genres(const vector<Genre>& genres) {
    for (auto& genre: genres) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(60, 5000);
        usleep(dis(gen));
        int pipe_fd;
        while((pipe_fd=open(genre.name.c_str(), O_RDWR)) == -1);
        string message = to_string(genre.count);
        write(pipe_fd, message.c_str(), message.size()+1);
        close(pipe_fd);
    }
}

vector<Genre> process_input(string inp, string& section){
    vector<Genre> genres;
    vector<string> split_inp = split(inp, ',');
    section = split_inp[split_inp.size()-1];
    split_inp.erase(split_inp.begin()+split_inp.size()-1);
    for (auto& genre: split_inp) {
        genres.push_back(Genre(genre));
    }
    return genres;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    if(log_level>=1)
        cout << "worker started..." << endl;
    string inp, section;
    cin >> inp;
    vector<Genre> genres = process_input(inp, section);
    count_genres_from_file(section, genres);
    report_genres(genres);
    return 0;
}