# Secure Client-Server Messaging Application

A secure client-server messaging application developed as part of my B.Sc. in Computer Science.

The project implements network communication between a C++ client and a Python server using TCP sockets, with cryptographic mechanisms for secure message exchange and persistent data storage.

## Features

- Client-server communication over TCP sockets
- Multi-client server using multithreading
- User registration and client management
- Secure message exchange between clients
- Public key exchange using RSA
- Message encryption using AES
- Persistent storage of client and message data
- Custom binary communication protocol

## Technologies

- C++
- Python
- TCP/IP Sockets
- Multithreading
- RSA Encryption
- AES Encryption
- Crypto++
- SQLite

## Project Structure

```text
├── client/              # C++ client implementation
├── server/              # Python server implementation
├── cryptopp_wrapper/    # RSA, AES and Base64 cryptographic wrappers
└── מסמך חולשות.docx     # Security analysis documentation
```

## Cryptography

The application combines asymmetric and symmetric cryptography for secure communication:

- **RSA** is used for public-key cryptography and key exchange.
- **AES** is used for symmetric message encryption.
- **Base64** encoding is used for representing cryptographic data.
- Cryptographic operations on the C++ client are implemented using the **Crypto++** library.

## Architecture

The application follows a client-server architecture:

- The **C++ client** handles user interaction, communication with the server, and client-side cryptographic operations.
- The **Python server** handles incoming client connections, processes protocol requests, manages multiple clients, and stores application data.
- **SQLite** provides persistent storage for client and message information.
- A custom communication protocol defines the structure of requests and responses exchanged between the client and server.

## Security

The project demonstrates the use of RSA and AES encryption as part of a secure messaging protocol.

A security analysis document is included in the repository and discusses identified vulnerabilities and security considerations in the implementation.

## About the Project

This project was developed as part of my B.Sc. in Computer Science at The Open University of Israel.

The project provided hands-on experience with socket programming, client-server architecture, multithreading, binary communication protocols, cryptography, and persistent data storage.