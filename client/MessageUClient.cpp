#include "MessageUClient.h"

std::vector<ClientData> client_data_list; //program list for clients details

static std::map<std::string, std::string> clients_list;
std::string my_symmetric_key;


//this function fill the header details by the values that get as a parameters.
static Header createHeader(const std::string& client_id,uint8_t version,uint16_t code, uint32_t payload_size)
{
	Header h{};
	std::fill(std::begin(h.client_id), std::end(h.client_id), ZERO);
	std::copy_n(client_id.begin(), std::min(client_id.size(), sizeof(h.client_id)), h.client_id);
	h.version = version;
	h.code = code;
	h.payload_size = payload_size;
	return h;
}

//this function check if client already exists by reading its id from file, if exists return his id, if file not exists of not opened return empty string.
static std::string checkIfExist()
{
	std::ifstream file("me.info", std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "the client file is not open" << std::endl;
		return "";
	}
	std::string user_n;
	std::getline(file, user_n);
	std::string client_id(CLIENT_ID_SIZE, '\0');
	file.read(&client_id[ZERO], CLIENT_ID_SIZE);
	file.close();
	return client_id;
}

//this function read the server answer, extracts the header fields and read the payload, return ans which is serveranser structure.
static ServerAnswer readServerAnswer(boost::asio::ip::tcp::socket& socket) {
	unsigned char header_buf[7];
	boost::asio::read(socket, boost::asio::buffer(header_buf, HEADER_SIZE));

	ServerAnswer ans{};
	ans.version = header_buf[ZERO];
	ans.code = header_buf[1] | (header_buf[2] << 8);
	ans.payload_size =
		header_buf[3] | (header_buf[4] << 8) |
		(header_buf[5] << 16) | (header_buf[6] << 24);

	if (ans.payload_size > ZERO) {
		ans.payload.resize(ans.payload_size);
		boost::asio::read(socket, boost::asio::buffer(ans.payload));
	}
	else {
		std::cout << "[debug] recieved empty payload not reading" << std::endl;
	}

	return ans;
}

//this function send the full request to server contains header and payload, combining them into one message and writing to the socket.
static void sendFullRequest(boost::asio::ip::tcp::socket& socket, const Header& header, const std::vector<unsigned char>& payload) {
	std::vector<unsigned char> msg;
	const unsigned char* headerBytes = reinterpret_cast<const unsigned char*>(&header);
	msg.insert(msg.end(), headerBytes, headerBytes + sizeof(Header));
	msg.insert(msg.end(), payload.begin(), payload.end());
	boost::asio::write(socket, boost::asio::buffer(msg));
}
//this function print the target id and the message id in hex format from server received message
static void printAnswerOfMessage(const std::vector<unsigned char>& payload) {
	std::string recv_client_id(payload.begin(), payload.begin() + CLIENT_ID_SIZE);
	std::string message_id(payload.begin() + CLIENT_ID_SIZE, payload.begin() + 20);

	std::cout << "Target Client ID: ";
	for (unsigned char c : recv_client_id)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c;

	std::cout << std::dec << "\nMessage ID: ";
	for (unsigned char c : message_id)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c;
	std::cout << std::dec << std::endl;
}
//this function convert the hex string into bytes vector
std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
	std::vector<unsigned char> bytes;
	for (unsigned int i = 0; i < hex.length(); i += 2) {
		std::string byteString = hex.substr(i, 2);
		unsigned char byte = (unsigned char)strtol(byteString.c_str(), nullptr, 16);
		bytes.push_back(byte);
	}
	return bytes;
}
//this function convert bytes vector into hex string
static std::string bytesToHex(const std::string& raw_id) {
	std::ostringstream oss;
	for (unsigned char c : raw_id)
		oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
	return oss.str();
}
//this function check if client has symmetric key already and return it . if not return empty string
static std::string getSymmetricKey(const std::string& sender_id) {
	for (auto& c : client_data_list)
		if (c.id == sender_id)
			return c.symmetric_key;

	return "";
}
//this function build the payload from the parameters get, this payload use for send request message of send message 
static std::vector<unsigned char> buildPayload(const std::string& dest_id,uint8_t message_type,const std::vector<unsigned char>& content){
	std::vector<unsigned char> payload;
	std::vector<unsigned char> dest_id_bytes = hex_to_bytes(dest_id);
	payload.insert(payload.end(), dest_id_bytes.begin(), dest_id_bytes.end());
	payload.push_back(message_type);
	uint32_t content_size = static_cast<uint32_t>(content.size());
	for (int i = 0; i < HEADER_PAYLOAD_SIZE; ++i)
		payload.push_back(static_cast<unsigned char>((content_size >> (8 * i)) & 0xFF));
	payload.insert(payload.end(), content.begin(), content.end());
	return payload;
}

//this function send request to server to get the users list, get the answer and parse into ids and names , if want to print like in function clients list than print the list
//the function take care to update the clients list in and client_data_list in memory.
bool updateClientsList(boost::asio::ip::tcp::socket& socket, bool wantPrint) {
	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return false;
	}
	Request req{};
	req.h= createHeader(client_id, VERSION, CODE_CLIENT_LIST, ZERO);

	boost::asio::write(socket, boost::asio::buffer(&req.h, sizeof(req.h)));
	ServerAnswer ans = readServerAnswer(socket);
	
	if (ans.code == CLIENTS_LIST_SUCCSESS) {
		if (wantPrint) {
			std::cout << "the client list is: \n";
		}
		clients_list.clear();
		for (size_t i = 0; i + CLIENT_SIZE <= ans.payload.size(); i += CLIENT_SIZE) {
			std::string id(ans.payload.begin() + i, ans.payload.begin() + i + CLIENT_ID_SIZE);
			std::string name(reinterpret_cast<char*>(&ans.payload[i + CLIENT_ID_SIZE]));
			name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
			if (wantPrint) {
				std::cout << "User: " << name << "\nClient ID: ";
				for (unsigned char c : id)
					std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c;
				std::cout << std::dec << "\n\n";
			}
			clients_list[name] = id;
		}

		for (size_t i = 0; i < ans.payload.size(); i += CLIENT_SIZE) {
			std::ostringstream oss;
			for (int j = 0; j < CLIENT_ID_SIZE; j++) {
				oss << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)ans.payload[i + j];
			}
			std::string id(oss.str());
			std::string name(reinterpret_cast<char*>(&ans.payload[i]), CLIENT_ID_SIZE);
			name.erase(std::find(name.begin(), name.end(), '\0'), name.end());

			ClientData cd;
			cd.name = name;
			cd.id = id;
			cd.public_key = "";
			cd.symmetric_key = "";
			
			auto it = std::find_if(client_data_list.begin(), client_data_list.end(), [&](const ClientData& old_cd) {
				return old_cd.id==id;
				});
			if (it == client_data_list.end()) {
				client_data_list.push_back(cd);
			}
		}

		std::cout << "Saved " << clients_list.size() << " users in memory beside you.\n";
		return true;
	}
	else {
		std::cerr << "server responded with an error" << std::endl;
		return false;
	}
	return true;
}
//this function responsible to request the user to insert name of client, and than find and return the id of the name the user insert.
std::string getDestId(boost::asio::ip::tcp::socket& socket) {
	std::string dest_name;
	std::cout << "please enter the destination client user name: \n";
	std::getline(std::cin, dest_name);

	if (!updateClientsList(socket, false)) {
		std::cout << "cannot update the clients list\n";
		return "";
	}
	auto it = clients_list.find(dest_name);
	if (it == clients_list.end()) {
		std::cout << "user not found \n";
		return "";
	}
	const std::string& raw_id = it->second;
	std::ostringstream oss;
	for (unsigned char c : raw_id)
		oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
	return oss.str();
}
//the main function , read the poet from file, connect to server and show the menu of option to user, after choise the function match the correct function and call it. 
int main() {
	try {
		std::ifstream file("server.info");
		if (!file.is_open()) {
			std::cerr << "couldnt open server info" << std::endl;
			return 1;
		}

		std::string line;
		std::getline(file, line);
		file.close();

		std::size_t pos = line.find(':');
		std::string host = line.substr(0, pos);
		int port = std::stoi(line.substr(pos + 1));

		boost::asio::io_context io;
		boost::asio::ip::tcp::resolver resolver(io);
		boost::asio::ip::tcp::socket socket(io);

		auto endpoints = resolver.resolve(host, std::to_string(port));
		boost::asio::connect(socket, endpoints);

		std::cout << "Connected to server at " << host << ":" << port << std::endl;


		while (true) {
			std::cout << "\nMessageU client at your service.\n";
			std::cout << "110) Register\n";
			std::cout << "120) Request for clients list\n";
			std::cout << "130) Request for public key\n";
			std::cout << "140) Request for waiting messages\n";
			std::cout << "150) Send a text message\n";
			std::cout << "151) Send a request for symmetric key\n";
			std::cout << "152) Send your symmetric key\n";
			std::cout << "153) Send a file\n";
			std::cout << "0) Exit client\n";
			std::cout << "?";

			int choice;
			std::cin >> choice;
			std::cin.ignore();

			if (choice == 0) {
				std::cout << "bye bye!" << std::endl;
				break;
			}
			int messageType = ZERO;
			switch (choice) {
			case 110: registerClient(socket); break;
			case 120: requestClientsList(socket); break;
			case 130: {
				std::string dest_id = getDestId(socket);
				if (dest_id.empty()) {
					break;
				}
				requestPublicKey(socket, dest_id);
				break;
			}
			case 140: requestWaitingMessages(socket); break;
			//when the function is to sent message , need to send the message type too.
			case 150:
				messageType = SEND_TEXT;
				sendTextMessage(socket, messageType); 
				break;
			case 151: 
				messageType = REQUEST_SYM_KEY;
				sendTextMessage(socket, messageType);
				break;
			case 152: 
				messageType = SEND_SYM_KEY;
				sendTextMessage(socket, messageType);
				break;
			case 153: 
				messageType = SEND_FILE;
				sendTextMessage(socket, messageType);
				break;
			default: std::cout << "there is no option like this. maybe try again...\n"; break;
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return 0;
}
//this function responsible for user that want to sign in. check if already exists nd if not, request the details, and make the sign in with server.
void registerClient(boost::asio::ip::tcp::socket& socket) {
	std::cout << "sign in to the system..." << std::endl;
	const std::string fileName = "me.info";
	if (std::filesystem::exists(fileName)) {
		std::cout << "you are already in system, not a register\n";
		return;
	}
	std::string user_name;
	while (true) {
		std::cout << "please enter your user name (up to 254 chars)" << std::endl;
		std::getline(std::cin >> std::ws, user_name);
		if (user_name.empty()) {
			std::cout << "you insert nothing , please enter again \n";
			continue;
		}
		if (user_name.size() > USERNAME_SIZE-1) {
			std::cout << "you insert name with over chars than allowed, please enter again \n";
			continue;
		}
		break;
	}
	//this part is for make a public key to the new user, we need to send it as a payload for server
	CryptoPP::AutoSeededRandomPool rng;

	CryptoPP::InvertibleRSAFunction privateKeyParams;
	privateKeyParams.Initialize(rng, RSA_KEY_SIZE); 

	CryptoPP::RSA::PrivateKey privateKey(privateKeyParams);
	CryptoPP::RSA::PublicKey publicKey(privateKeyParams);

	std::string publicKeyStr;
	CryptoPP::StringSink sink(publicKeyStr);
	publicKey.Save(sink);
	std::cout << "the public key size is : " << publicKeyStr.size();
	std::string privateKeyStr;

	std::vector<unsigned char> send_payload(USERNAME_SIZE+ PUBLIC_KEY_SIZE, 0);
	std::copy_n(user_name.begin(), std::min(user_name.size(),static_cast<size_t>(USERNAME_SIZE)), send_payload.begin());
	std::copy_n(publicKeyStr.begin(), std::min(publicKeyStr.size(), static_cast<size_t>(PUBLIC_KEY_SIZE)),send_payload.begin() + USERNAME_SIZE);

	Request req{};
	std::string empty_id(CLIENT_ID_SIZE, '\0');
	req.h = createHeader(empty_id, VERSION, CODE_REGISTER, static_cast<uint32_t>(send_payload.size()));
	req.payload = send_payload;

	sendFullRequest(socket, req.h, req.payload);
	//read server answer
	ServerAnswer ans = readServerAnswer(socket);
	//chekc if server return an succsess answer
	if (ans.code == REGISTER_SUCCSESS) {
		std::string client_id(ans.payload.begin(), ans.payload.end());

		std::cout << "Registration successful. Client ID: ";
		clients_list[user_name] = client_id;
		ClientData cd;
		cd.name = user_name;
		cd.id = client_id;
		cd.public_key = "";
		cd.symmetric_key = "";
		client_data_list.push_back(cd);
		for (unsigned char c : ans.payload)
			std::cout << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<int>(c);
		std::cout << std::dec << std::endl;
		//make a file for the new client
		std::ofstream file("me.info", std::ios::binary);
		file << user_name << "\n";
		file.write(reinterpret_cast<const char*>(ans.payload.data()), ans.payload.size());
		file << "\n" << privateKeyStr << std::endl;
		file.close();

		std::cout << "Saved user info to me.info\n";
	}
	else {
		std::cerr << "server responded with an error" << std::endl;
	}

}
// this function return and print the clients list with use of update client list function.
void requestClientsList(boost::asio::ip::tcp::socket& socket) {
	std::cout << "user request the client list..." << std::endl;
	updateClientsList(socket, true);//we use this to make sure that all clients updates in all lists and in data base.
}
//this function is for requsert public key, get the name of client , check if akready have public key, if not get from server.
std::string requestPublicKey(boost::asio::ip::tcp::socket& socket, const std::string& dest_id) {
	std::cout << "user request the public key..." << std::endl;
	std::cout << "the id for the name is : " << dest_id << std::endl;
	for (const auto& client : client_data_list) {
		if (client.id == dest_id && !client.public_key.empty()) {
			std::cout << "there is already public key for this user in RAM \n";
			return client.public_key;
		}
	}
	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return "";
	}
	std::vector<unsigned char> payload = hex_to_bytes(dest_id);
	Request req{};
	req.h.payload_size = static_cast<uint32_t>(payload.size());
	req.h = createHeader(client_id, VERSION, CODE_PUBLIC_KEY, static_cast<uint32_t>(payload.size()));

	std::cout << "sending payload (" << payload.size() << " bytes): ";
	for (unsigned char b : payload)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
	std::cout << std::dec << std::endl;
	sendFullRequest(socket, req.h, payload);
	
	ServerAnswer ans = readServerAnswer(socket);
	//check for the answer from server, if succeeded or not
	if (ans.code == PUBLIC_KEY_SUCSESS) {
		std::string get_id(ans.payload.begin(), ans.payload.begin() + CLIENT_ID_SIZE);
		std::string get_key(reinterpret_cast<const char*>(&ans.payload[CLIENT_ID_SIZE]), PUBLIC_KEY_SIZE);
		std::ostringstream oss;
		for (unsigned char c : get_id) {
			oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
		}//print the answer
		std::string get_id_hex = oss.str();
		std::cout << " id: " << get_id_hex;
		std::cout << std::dec << "\npublic key: " << get_key << "\n";
		for (auto& client : client_data_list) {
			if (client.id == get_id_hex) {
				client.public_key = get_key;
				break;
			}
		}
		
		return get_key;
	}
	else {
		std::cerr << "server responded with an error" << std::endl;
		return "";
	}
}
//this function responsible for show the waiting messages for each client , by request of user.
void requestWaitingMessages(boost::asio::ip::tcp::socket& socket) {
	std::cout << "user request waiting messages..." << std::endl;
	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return;
	}
	Request req{};
	req.h = createHeader(client_id, VERSION, CODE_WAITING_MESSAGES, ZERO);
	boost::asio::write(socket, boost::asio::buffer(&req.h, sizeof(req.h)));//send server a requset 
	ServerAnswer ans = readServerAnswer(socket);//serever check in database and return an answer, read it.
	if (ans.code == SERVER_ERROR || ans.payload_size == ZERO) {
		std::cout << "no messages waiting";
		return;
	}
	if (ans.code == WAITING_MESSAGES_SUCCSESS) {
		std::cout << "The waiting messages list:" << std::endl;
		//print the messages that waiting to be read by the client
		size_t offset = 0;
		while (offset + 25 <= ans.payload.size()) {
			std::string sender_id(ans.payload.begin() + offset, ans.payload.begin() + offset + CLIENT_ID_SIZE);
			offset += CLIENT_ID_SIZE;

			unsigned int msg_id = static_cast<unsigned int>(ans.payload[offset]) |
				(static_cast<unsigned int>(ans.payload[offset + 1]) << 8) |
				(static_cast<unsigned int>(ans.payload[offset + 2]) << 16) |
				(static_cast<unsigned int>(ans.payload[offset + 3]) << 24);
			offset += 4;

			unsigned char msg_type = ans.payload[offset];
			offset += 1;
			std::cout << "message type is : " << (int)msg_type << std::endl;

			unsigned int msg_size = static_cast<unsigned int>(ans.payload[offset]) |
				(static_cast<unsigned int>(ans.payload[offset + 1]) << 8) |
				(static_cast<unsigned int>(ans.payload[offset + 2]) << 16) |
				(static_cast<unsigned int>(ans.payload[offset + 3]) << 24);
			offset += 4;

			if (offset + msg_size > ans.payload.size()) break;

			std::string content(ans.payload.begin() + offset, ans.payload.begin() + offset + msg_size);
			offset += msg_size;

			switch (msg_type) {
			//print the message to user by the guidelines
			case REQUEST_SYM_KEY:
				std::cout << "Content: \nRequest for symmeric key" << std::endl;
				break;
			case SEND_SYM_KEY:
				std::cout << "Content: \nsymmetric key recieved" << std::endl;
				break;
			case SEND_TEXT: {
				std::string sender_id_hex = bytesToHex(sender_id);
				std::string key = getSymmetricKey(sender_id_hex);
				std::cout << "Content:\n";
				if (key.empty() && !my_symmetric_key.empty()) {
					key = my_symmetric_key;
				}
				if (key.empty()) {
					std::cout << "can't decrypt message\n";
					break;
				}
				try {
					//need to decrypt the message and show 
					AESWrapper aes(reinterpret_cast<const unsigned char*>(key.data()), static_cast<unsigned int>(key.size()));
					std::string ct(reinterpret_cast<const char*>(content.data()), content.size());
					std::string pt = aes.decrypt(content.data(), static_cast<unsigned int>(content.size()));
					std::cout << pt << "\n";
				}
				catch (...) {
					std::cout << "can't decrypt message\n";
				}
				break;
			}
			case SEND_FILE: {
				std::string key = getSymmetricKey(sender_id);
				std::cout << "Content:\n";
				if (key.empty() && !my_symmetric_key.empty()) {
					key = my_symmetric_key;
				}
				if (key.empty()) {
					std::cout << "can't decrypt message\n";
					break;
				}
				AESWrapper aes(reinterpret_cast<const unsigned char*>(key.data()), static_cast<unsigned int>(key.size()));
				std::string decrypted;
				try {
					//need to decrypt the file text
					decrypted = aes.decrypt(content.data(), static_cast<unsigned int>(content.size()));
				}
				catch (...) {
					std::cout << "cant decrypt the content. \n";
					break;
				}
#ifdef _WIN32
				char* tmpEnv = std::getenv("TMP");
				std::string tmp_path = tmpEnv ? std::string(tmpEnv) + "\\received_file.bin" : "C:\\Temp\\received_file.bin";
#else
				std::string tmp_path = "/tmp/received_file.bin";
#endif

				//write to the filw
				std::ofstream out(tmp_path, std::ios::binary);
				if (!out.is_open()) {
					std::cerr << "Error: could not create output file.\n";
					break;
				}
				out.write(decrypted.data(), decrypted.size());
				out.close();

				//show the place 
				std::cout <<tmp_path << "\n";
				break;
			}
			default:
				std::cout << "Content: unknown message type" << std::endl;
				break;
			}
			std::cout << ".\n";
			std::cout << ".\n";
			std::cout << "-----<EOM>-----\n";
			std::cout <<"\\n\n";
		}
	}
	else {
		std::cerr << "Server responded with an error. Code: " << ans.code << std::endl;
	}
}

//this function is for symmetric key requset, send server a request, get the answer and print it.
void symmetricKeyRequest(boost::asio::ip::tcp::socket& socket, const std::string& dest_id) {
	std::cout << "User requests to send a symmetric key..." << std::endl;
	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return;
	}

	std::vector<unsigned char> empty_con;
	std::vector<unsigned char> payload = buildPayload(dest_id, REQUEST_SYM_KEY, empty_con);
	Request req{};
	req.h = createHeader(client_id, VERSION, CODE_SEND_MESSAGE, PAYLOAD_EMPTY_MESSAGE);
	sendFullRequest(socket, req.h, payload);
	ServerAnswer ans = readServerAnswer(socket);
	if (ans.code == MESSAGE_SENT_SUCCSESS) {
		printAnswerOfMessage(ans.payload);
	}
	else {
		std::cerr << "Server responded with an error, code = " << ans.code << std::endl;
	}
}
//this function is for send symmetric key, generate symmetric key, check for public key, encrypt the symmetric key with public key, and send the request to answer
void sendSymmetricKey(boost::asio::ip::tcp::socket& socket, const std::string& dest_id ) {
	std::cout << "User requests to send a symmetric key..." << std::endl;

	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return;
	}
	//generate symmetric key
	AESWrapper aes;
	unsigned char buffer[AESWrapper::DEFAULT_KEYLENGTH];
	aes.GenerateKey(buffer, AESWrapper::DEFAULT_KEYLENGTH);
	std::string symmetric_key(reinterpret_cast<char*>(buffer), AESWrapper::DEFAULT_KEYLENGTH);

	std::string dest_public_key;
	for(auto& cdl : client_data_list) {
		if (cdl.id == dest_id) {
			dest_public_key = cdl.public_key;
			break;
		}
	}
	if (dest_public_key.empty()) {
		std::cerr << "there is no public key for this client" << std::endl;
		return;
	}
	std::cout << "dest_public_key size: " << dest_public_key.size() << std::endl;


	CryptoPP::RSA::PublicKey pub;
	CryptoPP::StringSource ss(dest_public_key, true);
	pub.BERDecode(ss);


	CryptoPP::AutoSeededRandomPool rng;
	CryptoPP::RSAES_OAEP_SHA_Encryptor enc(pub);

	std::string encrypted_key;
	CryptoPP::StringSource ss3(
		symmetric_key, true,
		new CryptoPP::PK_EncryptorFilter(rng, enc, new CryptoPP::StringSink(encrypted_key))
	);
	std::vector<unsigned char> encrypted_key_bytes(encrypted_key.begin(), encrypted_key.end());
	std::vector<unsigned char> payload = buildPayload(dest_id, 2, encrypted_key_bytes);
	Request req{};
	req.h = createHeader(client_id, VERSION, CODE_SEND_MESSAGE, static_cast<unsigned int>(payload.size()));
	sendFullRequest(socket, req.h, payload);

	ServerAnswer ans = readServerAnswer(socket);
	//server send the answer. check if succeeded or not
	if (ans.code == MESSAGE_SENT_SUCCSESS) {
		std::cout << "Symmetric key sent successfully." << std::endl;
		printAnswerOfMessage(ans.payload);

		std::ostringstream client_id_hex;
		for (int i = 0; i < CLIENT_ID_SIZE; i++) {
			client_id_hex << std::hex << std::setw(2) << std::setfill('0') << (int)req.h.client_id[i];
		}
		std::string client_id_str = client_id_hex.str();

		for (auto& client : client_data_list) {
			if (client.id == dest_id) {
				client.symmetric_key = symmetric_key;
				std::cout << "saved symmetric key in RAM memory for destination client\n";
			}
			my_symmetric_key = symmetric_key;
			std::cout << "saved symmetric key in RAM memory for sender client\n";
		}
	}
	else {
		std::cerr << "Server responded with an error (code " << ans.code << ")." << std::endl;
	}
}
//this function send text message to other client.
void sendText(boost::asio::ip::tcp::socket& socket, const std::string& dest_id) {
	std::cout << "User requests to send a text message..." << std::endl;

	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return;
	}
	//insert the message that want to send
	std::string plain_text;
	std::cout << "please enter the text massage that you would like to send: ";
	std::getline(std::cin, plain_text);

	std::string symmetric_key = getSymmetricKey(dest_id);
	bool is_key = false;
	if (!symmetric_key.empty())
	{
		is_key = true;
	}

	if (!is_key || symmetric_key.empty()) {
		std::cout << "there is no symmetric key already, please use 152 to get symmetric key for this action \n";
		return;
	}

	//encrypt the text with the symmetric key
	AESWrapper aes(reinterpret_cast<const unsigned char*>(symmetric_key.c_str()),
		AESWrapper::DEFAULT_KEYLENGTH);

	std::string encrypted_msg = aes.encrypt(const_cast<char*>(plain_text.c_str()),
		static_cast<unsigned int>(plain_text.length()));


	std::vector<unsigned char> encrypted_text_bytes(encrypted_msg.begin(), encrypted_msg.end());
	std::vector<unsigned char> payload = buildPayload(dest_id, 3, encrypted_text_bytes);
	Request req{};
	req.h = createHeader(client_id, VERSION, CODE_SEND_MESSAGE, static_cast<unsigned int>(payload.size()));
	sendFullRequest(socket, req.h, payload);

	std::cout << "Encrypted text message sent successfully!" << std::endl;
	ServerAnswer ans = readServerAnswer(socket);
	//if succeeded print the answer
	if (ans.code == MESSAGE_SENT_SUCCSESS) {
		printAnswerOfMessage(ans.payload);
	}
	else {
		std::cerr << "Server responded with an error (code " << ans.code << ")." << std::endl;
	}
}

//this function is to send a file to other client. 
void sendFile(boost::asio::ip::tcp::socket& socket, const std::string& dest_id) {
	std::cout << "User requests to send a file..." << std::endl;

	std::string client_id = checkIfExist();
	if (client_id.empty()) {
		return;
	}

	std::string filepath;
	std::cout << "Enter full path of file to send: ";
	std::getline(std::cin, filepath);

	std::ifstream in_file(filepath, std::ios::binary);
	if (!in_file.is_open()) {
		std::cerr << "file not found" << std::endl;
		return;
	}

	std::vector<unsigned char> file_data(
		(std::istreambuf_iterator<char>(in_file)),
		std::istreambuf_iterator<char>()
	);
	in_file.close();

	std::string symmetric_key= getSymmetricKey(dest_id);
	if (symmetric_key.empty()) {
		std::cerr << "No symmetric key found for this client. Send one first (option 152)." << std::endl;
		return;
	}

	AESWrapper aes(reinterpret_cast<const unsigned char*>(symmetric_key.c_str()),
		AESWrapper::DEFAULT_KEYLENGTH);

	std::string encrypted_file = aes.encrypt(
		reinterpret_cast<const char*>(file_data.data()),
		static_cast<unsigned int>(file_data.size())
	);

	std::vector<unsigned char> encrypted_file_bytes(encrypted_file.begin(), encrypted_file.end());
	std::vector<unsigned char> payload = buildPayload(dest_id, SEND_FILE, encrypted_file_bytes);
	Request req{};
	req.h = createHeader(client_id, VERSION_FILE, CODE_SEND_MESSAGE, CLIENT_ID_SIZE + MESSAGE_TYPE_SIZE + CONTENT_SIZE_BYTES + static_cast<unsigned int>(encrypted_file.size()));
	sendFullRequest(socket, req.h, payload);

	std::cout << "File '" << filepath << "' sent successfully (" << file_data.size() << " bytes)." << std::endl;
	ServerAnswer ans = readServerAnswer(socket);
	//check if succeeded
	if (ans.code == MESSAGE_SENT_SUCCSESS) {
		std::cout << "Symmetric key sent successfully." << std::endl;
		printAnswerOfMessage(ans.payload);
		for (auto& client : client_data_list) {
			if (client.id == dest_id) {
				client.symmetric_key = symmetric_key;
				std::cout << "saved symmetric key in RAM memory \n";
				break;
			}
		}
	}
	else {
		std::cerr << "Server responded with an error (code " << ans.code << ")." << std::endl;
	}
}

//this function check what is the type of the message and then send to the match function
void sendTextMessage(boost::asio::ip::tcp::socket& socket, int messageType) {
	std::cout << "user requset send text message..." << std::endl;
	std::string dest_id = getDestId(socket);
	if (dest_id.empty()) {
		return;
	}
	switch (messageType) {
	case 1:
		symmetricKeyRequest(socket, dest_id);
		break;
	case 2:
		sendSymmetricKey(socket, dest_id);
		break;
	case 3:
		sendText(socket, dest_id);
		break;
	case 4:
		sendFile(socket, dest_id);
		break;
	default:
		std::cout << "the type of message is illegal" << std::endl;
	}
}