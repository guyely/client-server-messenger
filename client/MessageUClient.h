#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <boost/asio.hpp>
#include "cryptlib.h"
#include "rsa.h"
#include "osrng.h"
#include "base64.h"
#include "cryptopp_wrapper/AESWrapper.h"
#include "cryptopp_wrapper/RSAWrapper.h"
#include "cryptopp_wrapper/Base64Wrapper.h"

//define all numbers in code
#define VERSION 1
#define VERSION_FILE 2
#define CODE_REGISTER 600
#define CODE_CLIENT_LIST 601
#define CODE_PUBLIC_KEY 602
#define CODE_SEND_MESSAGE 603
#define CODE_WAITING_MESSAGES 604
#define REGISTER_SUCCSESS 2100
#define CLIENTS_LIST_SUCCSESS 2101
#define PUBLIC_KEY_SUCSESS 2102
#define MESSAGE_SENT_SUCCSESS 2103
#define WAITING_MESSAGES_SUCCSESS 2104
#define SERVER_ERROR 9000
#define REQUEST_SYM_KEY 1
#define SEND_SYM_KEY 2
#define SEND_TEXT 3
#define SEND_FILE 4
#define HEADER_SIZE 7
#define HEADER_PAYLOAD_SIZE 4
#define CLIENT_ID_SIZE 16
#define USERNAME_SIZE 255
#define PUBLIC_KEY_SIZE 160
#define CLIENT_SIZE 271
#define RSA_KEY_SIZE 1024
#define CONTENT_SIZE_BYTES 4
#define MESSAGE_TYPE_SIZE 1
#define PAYLOAD_EMPTY_MESSAGE 21
#define ZERO 0

//struct for header in messages
#pragma pack(push,1)
struct Header {
	unsigned char client_id[CLIENT_ID_SIZE];
	uint8_t version;
	uint16_t code;
	uint32_t payload_size;
};
#pragma pack(pop)

//struct for full request contains header and payload
#pragma pack(push,1)
struct Request {
	Header h;
	std::vector<unsigned char> payload;
};
#pragma pack(pop)

//struct for clients details in program
struct ClientData {
	std::string name;
	std::string id;
	std::string public_key;
	std::string symmetric_key;
};

//struct for answer gets from server
struct ServerAnswer {
	uint8_t version;
	uint16_t code;
	uint32_t payload_size;
	std::vector<unsigned char> payload;
}; 
#pragma once

void registerClient(boost::asio::ip::tcp::socket& socket);
void requestClientsList(boost::asio::ip::tcp::socket& socket);
std::string requestPublicKey(boost::asio::ip::tcp::socket& socket, const std::string& dest_id);
void requestWaitingMessages(boost::asio::ip::tcp::socket& socket);
void sendTextMessage(boost::asio::ip::tcp::socket& socket, int messageType);
Header createHeader(const std::string& client_id, uint8_t version, uint16_t code, uint32_t payload_size);
std::string checkIfExist();
ServerAnswer readServerAnswer(boost::asio::ip::tcp::socket& socket);
void sendFullRequest(boost::asio::ip::tcp::socket& socket, const Header& header, const std::vector<unsigned char>& payload);
void printAnswerOfMessage(const std::vector<unsigned char>& payload);
std::vector<unsigned char> hex_to_bytes(const std::string& hex);
std::string bytesToHex(const std::string& raw_id);
std::string getSymmetricKey(const std::string& sender_id);
std::vector<unsigned char> buildPayload(const std::string& dest_id, uint8_t message_type, const std::vector<unsigned char>& content);
bool updateClientsList(boost::asio::ip::tcp::socket& socket, bool wantPrint);
std::string getDestId(boost::asio::ip::tcp::socket& socket);
void symmetricKeyRequest(boost::asio::ip::tcp::socket& socket, const std::string& dest_id);
void sendSymmetricKey(boost::asio::ip::tcp::socket& socket, const std::string& dest_id);
void sendText(boost::asio::ip::tcp::socket& socket, const std::string& dest_id);
void sendFile(boost::asio::ip::tcp::socket& socket, const std::string& dest_id);

