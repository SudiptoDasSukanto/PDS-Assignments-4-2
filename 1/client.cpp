#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;

atomic<bool> running(true);
mutex mtx;
condition_variable cv;
bool ready_to_send = false;

string trim(const string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

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

string recv_line(int sock) {
    string res;
    char c;
    while (true) {
        ssize_t r = recv(sock, &c, 1, 0);
        if (r <= 0) return "";
        if (c == '\n') break;
        res.push_back(c);
        if (res.size() > 2000) break;
    }
    return trim(res);
}

// Check if more data is available to read (non-blocking check)
bool has_pending_data(int sock) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms timeout
    
    int result = select(sock + 1, &readfds, nullptr, nullptr, &timeout);
    return result > 0 && FD_ISSET(sock, &readfds);
}

void receive_messages(int sock) {
    while (running) {
        string server_msg = recv_line(sock);
        if (server_msg.empty()) {
            lock_guard<mutex> lock(mtx);
            cout << "Server closed connection.\n";
            running = false;
            cv.notify_all();
            break;
        }

        // Print the server's message immediately
        {
            lock_guard<mutex> lock(mtx);
            cout << "Server: " << server_msg << "\n";

            // Check for termination message
            if (server_msg == "I have no more jokes to tell.") {
                cout << "No more jokes. Closing client.\n";
                running = false;
                cv.notify_all();
                break;
            }
        }

        // NEW LOGIC: Check if more messages are coming
        // Wait a bit and see if server sends more messages
        bool more_messages = true;
        while (more_messages && running) {
            if (has_pending_data(sock)) {
                // There's more data, read the next message
                string next_msg = recv_line(sock);
                if (next_msg.empty()) {
                    lock_guard<mutex> lock(mtx);
                    cout << "Server closed connection.\n";
                    running = false;
                    cv.notify_all();
                    return;
                }
                
                {
                    lock_guard<mutex> lock(mtx);
                    cout << "Server: " << next_msg << "\n";
                    
                    if (next_msg == "I have no more jokes to tell.") {
                        cout << "No more jokes. Closing client.\n";
                        running = false;
                        cv.notify_all();
                        return;
                    }
                }
            } else {
                // No more immediate messages, ready for user input
                more_messages = false;
            }
        }

        // Only after receiving ALL server messages, allow user input
        if (running) {
            lock_guard<mutex> lock(mtx);
            ready_to_send = true;
            cv.notify_all();
        }
    }
}

void send_messages(int sock) {
    while (running) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return ready_to_send || !running; });

        if (!running) break;

        cout << "Client: ";
        string user_input;
        if (!std::getline(cin, user_input)) {
            send_line(sock, "N");
            running = false;
            break;
        }

        user_input = trim(user_input);
        if (!send_line(sock, user_input)) {
            cout << "Failed to send. Exiting.\n";
            running = false;
            break;
        }

        ready_to_send = false;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <server-ip> <port>\n";
        return 1;
    }
    string server_ip = argv[1];
    int port = stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &servaddr.sin_addr) <= 0) {
        cerr << "Invalid address\n";
        return 1;
    }

    if (connect(sock, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        return 1;
    }

    cout << "Connected to server " << server_ip << ":" << port << "\n";

    thread receiver(receive_messages, sock);
    thread sender(send_messages, sock);

    receiver.join();
    sender.join();

    close(sock);
    return 0;
}