# **Knock-Knock Joke Server and Client**

This project implements a simple **Knock-Knock Joke Server and Client** using socket programming. The server sends jokes to the client, validates responses, and handles multiple interactions. The client interacts with the server by responding to prompts and receiving punchlines.

This project was developed on **Windows** using **WSL (Windows Subsystem for Linux)**.

---

## **How to Run**

### **Prerequisites**
1. Install **WSL** on your Windows system.
2. Install a Linux distribution (e.g., Ubuntu) via WSL.
3. Ensure the following tools are installed in WSL:
   - `g++` (for compiling the code)
   - `make` (for building the project)

### **Steps to Run Same Machine**
1. Open a terminal in WSL and navigate to the project directory.
2. Build the project using:
   ```bash
   make
   ```
3. Start the server by running:
   ```bash
   ./server 9000 jokes.txt
   ```
   - `9000` is the port number the server will listen on.
   - [`jokes.txt`](jokes.txt ) is the file containing the jokes (in the format: setup and punchline pairs).

4. Open a new terminal in WSL and start the client by running:
   ```bash
   ./client 127.0.0.1 9000
   ```
   - `127.0.0.1` is the localhost IP address.
   - `9000` is the port number the client will connect to.

---
### **Steps to Run Different Machine**

- **Your PC (Server)**
1. sds@SDSpc:~$ 
```bash
hostname -I
```
   - 192.168.1.100

3. sds@SDSpc:~$ 
```bash
./server 9000 jokes.txt
```
   - Loaded 15 jokes.
   - Server listening on port 9000

- **Other PC (Client):**
1. friend@OtherPC:~$ 
```bash
g++ -pthread -std=c++17 client.cpp -o client
```
2. friend@OtherPC:~$ 
```bash 
./client 192.168.1.100 9000
```
   - Connected to server 192.168.1.100:9000
   - Server: Knock knock!
   - Client: Who's there?

---

## **Features**
- **Server**:
  - Reads jokes from a file ([`jokes.txt`](jokes.txt )) and serves them to clients.
  - Validates client responses and provides appropriate feedback.
  - Handles multiple interactions with the client, including restarting jokes when necessary.
  - Closes the connection gracefully when all jokes are served.

- **Client**:
  - Connects to the server and interacts by responding to prompts.
  - Receives jokes and punchlines sequentially.
  - Handles server messages and displays them to the user.

---

## **File Structure**
- **[`server.cpp`](server.cpp )**: Implements the server logic.
- **[`client.cpp`](client.cpp )**: Implements the client logic.
- **[`jokes.txt`](jokes.txt )**: Contains the jokes in the format:
  ```
  Setup
  Punchline
  ```
- **[`Makefile`](Makefile )**: Automates the build process for the server and client.

---