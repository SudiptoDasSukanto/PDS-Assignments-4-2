// server.cpp
// Compile: g++ -pthread -std=c++17 server.cpp -o server

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#define BACKLOG 10

using namespace std;

// Global jokes database: vector of {setup, punchline}
vector<pair<string, string>> jokes;

// Active client counter (just for info)
atomic<int> active_clients(0);
mutex cout_mtx;

string trim(const string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Case-insensitive comparison (but spelling-sensitive)
bool iequals(const string &a, const string &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

// Send string with newline
bool send_line(int sock, const string &s) {
    string out = s + "\n";
    const char *buf = out.c_str();
    size_t left = out.size();
    while (left > 0) {
        ssize_t sent = send(sock, buf, left, 0);
        if (sent <= 0) return false;
        buf += sent;
        left -= sent;
    }
    return true;
}

// Receive line until '\n'
string recv_line(int sock) {
    string res;
    char c;
    while (true) {
        ssize_t r = recv(sock, &c, 1, 0);
        if (r <= 0) return "";
        if (c == '\n') break;
        res.push_back(c);
        if (res.size() > 2000) break; // safety
    }
    return trim(res);
}

// Handle one client
void client_thread(int client_sock, sockaddr_in client_addr) {
    active_clients.fetch_add(1);
    {
        lock_guard<mutex> lg(cout_mtx);
        cout << "Client connected: "
             << inet_ntoa(client_addr.sin_addr) << ":"
             << ntohs(client_addr.sin_port) << "\n";
    }

    // Random order of jokes for this client
    vector<int> idx(jokes.size());
    for (size_t i = 0; i < jokes.size(); ++i) idx[i] = (int)i;
    random_device rd;
    mt19937 g(rd());
    shuffle(idx.begin(), idx.end(), g);

    size_t next_joke_pos = 0;

    while (true) {
        if (next_joke_pos >= idx.size()) {
            send_line(client_sock, "I have no more jokes to tell.");
            break;
        }

        int jidx = idx[next_joke_pos];
        const string setup = jokes[jidx].first;
        const string punch = jokes[jidx].second;

        // Start joke
        if (!send_line(client_sock, "Knock knock!")) break;

        string resp = recv_line(client_sock);
        if (resp.empty()) break;

        // Step 1: Expect "Who's there?"
        if (!iequals(resp, "Who's there?")) {
            send_line(client_sock,
                      "You are supposed to say, \"Who's there?\". Let's try again.");
            continue; // restart joke
        }

        // Step 2: Send setup
        string setup_msg = setup;
        if (!setup_msg.empty() && setup_msg.back() != '.' && setup_msg.back() != '!')
            setup_msg += ".";
        if (!send_line(client_sock, setup_msg)) break;

        resp = recv_line(client_sock);
        if (resp.empty()) break;

        // Step 3: Expect "<setup> who?"
        string expected = setup + " who?";
        while (!iequals(resp, expected)) {
            string remind = "You are supposed to say, \"" + expected +
                            "\". Let's try again.";
            send_line(client_sock, remind);
            send_line(client_sock, "Knock knock!");

            resp = recv_line(client_sock);
            if (resp.empty()) break;

        }

        // Step 4: Send punchline
        send_line(client_sock, punch);
        send_line(client_sock, "Would you like to listen to another? (Y/N)");
        
        // if (!send_line(client_sock, punch)) break;

        // // Ask for another
        // if (!send_line(client_sock, "Would you like to listen to another? (Y/N)"))
        //     break;

        resp = recv_line(client_sock);
        if (resp.empty()) break;

        if (iequals(resp, "Y")) {
            next_joke_pos++;
            continue;
        } else {
            break; // client chose to stop
        }
    }

    close(client_sock);
    active_clients.fetch_sub(1);
    {
        lock_guard<mutex> lg(cout_mtx);
        cout << "Client disconnected: "
             << inet_ntoa(client_addr.sin_addr) << ":"
             << ntohs(client_addr.sin_port) << "\n";
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <port> [jokes_file]\n";
        return 1;
    }
    int port = stoi(argv[1]);
    string jokes_file = (argc >= 3 ? argv[2] : "jokes.txt");

    // Load jokes
    ifstream fin(jokes_file);
    if (!fin) {
        cerr << "Failed to open jokes file: " << jokes_file << "\n";
        return 1;
    }
    string line1, line2;
    while (getline(fin, line1)) {
        if (!getline(fin, line2)) break;
        line1 = trim(line1);
        line2 = trim(line2);
        if (!line1.empty()) {
            jokes.emplace_back(line1, line2);
        }
    }
    fin.close();

    if (jokes.empty()) {
        cerr << "No jokes found in " << jokes_file << "\n";
        return 1;
    }
    cout << "Loaded " << jokes.size() << " jokes.\n";
    if (jokes.size() < 15) {
        cerr << "Warning: assignment requires at least 15 jokes. Current: "
             << jokes.size() << "\n";
    }

    // Create server socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(sockfd, BACKLOG) < 0) {
        perror("listen");
        return 1;
    }

    cout << "Server listening on port " << port << "\n";

    // Accept clients forever
    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 60; // 1 minute timeout
        timeout.tv_usec = 0;

        int activity = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout);

        if (activity < 0) {
            perror("select");
            break;
        } else if (activity == 0) {
            // Timeout occurred, check if there are active clients
            if (active_clients == 0) {
                cout << "No clients connected for 1 minute. Server shutting down.\n";
                break;
            }
        } else {
            // New client connection
            sockaddr_in cliaddr{};
            socklen_t addrlen = sizeof(cliaddr);
            int client_sock = accept(sockfd, (sockaddr *)&cliaddr, &addrlen);
            if (client_sock < 0) {
                perror("accept");
                continue;
            }
            std::thread t(client_thread, client_sock, cliaddr);
            t.detach(); // don't block server, let thread run independently
        }
    }

    close(sockfd);
    return 0;
}
