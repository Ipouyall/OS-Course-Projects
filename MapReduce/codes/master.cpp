#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <strings.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>

#define GENRES_FILE "genres.csv"
#define AGGREGATOR_EXECUTABLE "./aggregator.out"
#define WORKER_EXECUTABLE "./worker.out"
#define READ 0
#define WRITE 1

using namespace std;

const int log_level = 0;

vector<string> split(const string& str, char delimiter) {
    vector<string> internal;
    stringstream ss(str);
    string tok;

    while(getline(ss, tok, delimiter)) {
        internal.push_back(tok);
    }

    return internal;
}

vector<string> get_csv_contents(const string& filename) {
    ifstream file(filename);
    vector<string> contents;
    string line;

    while (getline(file, line)) {
        contents.push_back(line);
    }
    return contents;
}

vector<string> read_genres(const string& dir) {
    string file_name = dir + "/" + GENRES_FILE;
    vector<string> contents = get_csv_contents(file_name);
    vector<string> genres;
    for (auto& line : contents) {
        vector<string> split_line = split(line, ',');
        for (auto& genre: split_line) {
            genres.push_back(genre);
        }

    }
    return genres;
}

vector<string> all_files_exists(const string& directory){
    vector<string> files;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (directory.c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                files.emplace_back(ent->d_name);
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror ("");
    }
    return files;
}

vector<string> filter_files(const vector<string>& files, const string& has) {
    vector<string> filtered_files;
    for (auto& file : files) {
        if (file.find(has) != string::npos) {
            filtered_files.push_back(file);
        }
    }
    return filtered_files;
}

vector<string> add_dir_to_path(const string& directory, const vector<string>& files) {
    vector<string> files_with_path;
    for (auto& file : files) {
        files_with_path.push_back(directory + "/" + file);
    }
    return files_with_path;
}

int run_new_process(const string& executable, int& write_pipe) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
    }
    int pid = fork();
    if (pid == 0) {
        // Child process
        dup2(pipe_fd[READ], STDIN_FILENO);
        close(pipe_fd[WRITE]);
        close(pipe_fd[READ]);
        execl(executable.c_str(), executable.c_str(), NULL);
        perror("execl");
    } else if (pid > 0) {
        // Parent process
        close(pipe_fd[READ]);
        write_pipe = pipe_fd[WRITE];
    }else{
        perror("fork");
    }
    return pid;
}

string genre_to_string(const vector<string>& genres) {
    string genre_string;
    for (auto& genre : genres) {
        genre_string += genre + ",";
    }
    return genre_string;
}


int main(int argc, char* argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    if(log_level >= 1)
        cout << "Master starting..." << endl;
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <path>" << endl;
        return 1;
    }
    string directory = argv[1];
    vector<string> genres = read_genres(directory);
    vector<string> library_parts = add_dir_to_path(
            directory ,
            filter_files(
                    filter_files(all_files_exists(directory),".csv"),"part"));
    vector<int> child_pids;
    int worker_count = library_parts.size();

    for(auto& genre : genres) {
        if(log_level >= 1)
            cout << "Master: Genre " << genre << endl;
        int aggregator_write_pipe;
        int aggregator_pid = run_new_process(AGGREGATOR_EXECUTABLE, aggregator_write_pipe);
        if(aggregator_pid <= 0)
            return 0;
        string data = to_string(worker_count) + "," + genre;
        write(aggregator_write_pipe, data.c_str(), data.size());
        child_pids.push_back(aggregator_pid);
        close(aggregator_write_pipe);
    }
    string genre_str = genre_to_string(genres);
    for(auto& library_part : library_parts) {
        if(log_level >= 1)
            cout << "Master: Library part " << library_part << endl;
        int worker_write_pipe;
        int worker_pid = run_new_process(WORKER_EXECUTABLE, worker_write_pipe);
        if(worker_pid <= 0)
            return 0;
        string data = genre_str+library_part;
        write(worker_write_pipe, data.c_str(), data.size());
        child_pids.push_back(worker_pid);
        close(worker_write_pipe);
    }
    // Wait for all children to finish
    for(auto& pid : child_pids) {
        int status;
        waitpid(pid, &status, 0);
        if(log_level>=2)
            cout << "Master: Child " << pid << " finished with status " << WEXITSTATUS(status) << endl;
    }
    return 0;
}