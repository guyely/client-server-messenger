import socket
import struct
import uuid
import sqlite3
import threading
from db import DB_FILE_NAME
from db import init_db


HEADER_FMT = "<16sBHI"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

HOST = "0.0.0.0"
DEFAULT_PORT = 1357 #if the read of port is not succeeded we use this port

#this function get the number of bytes from socket , read this number until received all or connection closed.
def recv_exact(conn, size: int)->bytes:
    data = b""
    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            raise ConnectionError("connection closed")
        data += chunk
    return data


#this function check the massage size, parse the message to payload and rest(if exists) and return the values by dictionary.
def parse_message(data: bytes):
    if len(data) < HEADER_SIZE:
        raise ValueError("message too short, the header not contain all bytes")
    client_id, version, code, payload_size = struct.unpack(HEADER_FMT, data[:HEADER_SIZE])
    if len(data) - HEADER_SIZE < payload_size:
        raise ValueError("payload is too short")
    payload = data[HEADER_SIZE:HEADER_SIZE + payload_size]
    rest = data[HEADER_SIZE + payload_size:]

    return {
        "client_id": client_id,
        "version": version,
        "code": code,
        "payload_size": payload_size,
        "payload": payload,
        "rest": rest,
    }

#this function send the answer to client, the header and the payload, writing them into the socket.
def send_answer(conn ,code , payload = b""):
    version = 2
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    payload_size = len(payload)
    header_fmt = "<BHI"
    header = struct.pack(header_fmt, version, code, payload_size)
    try:
        conn.sendall(header)
        if payload_size > 0:
            conn.sendall(payload)
        conn.setblocking(True)
    except Exception as e:
        print(f"error sending answer: {e}")

#this function get the client data from client, check for duplicates and saving client details in database.
def handle_register(data, conn):
    try:
        payload = data["payload"] if isinstance(data,dict) else data
        username_bytes = payload[:255]
        username = username_bytes.decode("utf-8", errors= "ignore").rstrip('\x00')
        public_key = payload[255:255+160]

        db_conn = sqlite3.connect(DB_FILE_NAME)
        c = db_conn.cursor()

        c.execute("SELECT ID FROM clients WHERE Username = ?", (username,))
        row = c.fetchone()
        if row:
            print(f"Registration failed: username '{username}' already exists")
            send_answer(conn, 9000)
            db_conn.close()
            return
        #create id for this client
        client_id = uuid.uuid4().bytes

        c.execute("""
            INSERT INTO clients (ID, Username, PublicKey, LastSeen)
            VALUES (?, ?, ?, datetime('now'))
        """, (client_id, username, public_key))
        db_conn.commit()
        db_conn.close()

        send_answer(conn, 2100, client_id)
        print(f"Registered new client: {username}, uuid={client_id.hex()}")

    except Exception as e:
        print("Error in handle_register:", e)
        send_answer(conn, 9000)

#this function return to client all the clients in database beside the client that ask for the list. the list contains the ids and usernames.
def handle_clients_list(msg,conn):
    try:
        client_id = msg["client_id"]
        db_conn = sqlite3.connect(DB_FILE_NAME)
        c = db_conn.cursor()
        c.execute("SELECT ID, Username FROM clients WHERE ID != ?", (client_id,))
        rows = c.fetchall()
        db_conn.close()

        payload = b""
        for row in rows:
            client_id = row[0]
            username = row[1].encode("ascii") + b'\x00'
            username = username.ljust(255, b'\x00')
            payload += client_id + username

        send_answer(conn, 2101 , payload)
        print(f"Sent clients list with {len(rows)} users")

    except Exception as e:
        print("Error in handle_clients_list:", e)
        send_answer(conn, 9000, b"")

#this function get from client the client id , search for public key in database , if exists send it, else send error code.
def handle_public_key(msg, conn):
    try:
        payload = msg["payload"]
        client_id = payload[:16]

        db_conn = sqlite3.connect(DB_FILE_NAME)
        c = db_conn.cursor()
        c.execute("SELECT PublicKey FROM clients WHERE ID=?", (client_id,))
        row = c.fetchone()
        db_conn.close()

        if not row:
            send_answer(conn, 9000, payload=b"")
            print("Public key request failed – client not found")
            return

        public_key = row[0]
        answer = client_id + public_key
        send_answer(conn, 2102, answer)
        print("Sent public key (len=%d bytes)" % len(public_key))

    except Exception as e:
        print("Error in handle_public_key:", e)
        send_answer(conn, 9000, b"")

#this function get message details from client , insert into database , and print message for success. if not succeeded return error code.
def handle_send_message(msg,conn):
    try:
        payload = msg["payload"]
        if not payload or len(payload) < 21:
            raise ValueError(f"Payload too short: {len(payload)}")
        from_client = msg["client_id"]
        to_client = payload[0:16]
        msg_type = int.from_bytes(payload[16:17],"little")
        content_size = int.from_bytes(payload[17:21], "little")
        content = payload[21:21 + content_size]
        db_conn = sqlite3.connect(DB_FILE_NAME)
        c = db_conn.cursor()

        c.execute("""
            INSERT INTO messages (ToClient, FromClient, Type, Content)
            VALUES (?, ?, ?, ?)
            """, (to_client, from_client, msg_type, content))
        message_id = c.lastrowid
        db_conn.commit()
        db_conn.close()

        print(f"Stored message from {from_client.hex()} to {to_client.hex()}")
        payload = to_client + message_id.to_bytes(4, "little")
        send_answer(conn, 2103, payload)

    except Exception as e:
        print("Error in handle_send_message:", e)
        send_answer(conn, 9000, b"")

#this function get a request for waiting messages from client, take the message for him from database, send him, and then delete these messages from database.
def handle_waiting_messages(msg,conn):
    try:
        client_id = msg.get("client_id")
        if isinstance(client_id, str):
            client_id = bytes.fromhex(client_id)
        db_conn = sqlite3.connect(DB_FILE_NAME)
        c = db_conn.cursor()
        c.execute("SELECT ID, Type, Content FROM messages WHERE ToClient = ?", (client_id,))
        messages = c.fetchall()
        #print("got messages table:", messages)
        if not messages:
            send_answer(conn , 9000 , b"")
            db_conn.close()
            return
        payload = b""
        for msg_id, msg_type, content in messages:
            content = content if isinstance(content, bytes) else content.encode("utf-8")
            payload+=client_id
            payload+= msg_id.to_bytes(4, "little")
            payload += msg_type.to_bytes(1, "little")
            payload+= len(content).to_bytes(4, "little")
            payload += content

        send_answer(conn, 2104, payload=payload)

        c.execute("DELETE FROM messages WHERE ToClient = ?", (client_id,))
        db_conn.commit()
        db_conn.close()

    except Exception as e:
        print("Error in handle_waiting_messages:", e)
        send_answer(conn, 9000, b"")

#this function check what is the code that client chose and call the function that match the code.
def requests(msg,conn):
    code = msg["code"]
    if code == 600:
        handle_register(msg,conn)
    elif code == 601:
        handle_clients_list(msg,conn)
    elif code == 602:
        handle_public_key(msg, conn)
    elif code == 603:
        handle_send_message(msg,conn)
    elif code == 604:
        handle_waiting_messages(msg,conn)
    else:
        print("unknown code:", code)

#this function parse the details, send to parse message for call the match function .
def handle_client(conn,addr):
    try:
        while True:
            header_bytes = recv_exact(conn, HEADER_SIZE)
            if not header_bytes:
                break
            client_id, version, code, payload_size = struct.unpack(HEADER_FMT, header_bytes)
            payload_bytes = recv_exact(conn, payload_size)
            msg = parse_message(header_bytes + payload_bytes)
            requests(msg, conn)
    except ConnectionError:
        print("Connection closed")
    finally:
        conn.close()

#this function start the server,read the port from file(or use default) , listen for new connections and make a new thread for each client.
def start_server():
    try:
        with open("myport.info") as f:
            port = int(f.readline())
    except FileNotFoundError:
        print("file not found... using default port")
        port = DEFAULT_PORT

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, port))
        s.listen()
        print(f"Waiting for connection on port {port}...")
        while True:
            conn, addr = s.accept()
            print("connection from", addr)
            thread = threading.Thread(target= handle_client, args=(conn, addr))
            thread.start()

#main function
if __name__ == '__main__':
    init_db()
    print("the database is ready")
    start_server()
