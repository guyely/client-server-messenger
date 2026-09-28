import sqlite3

DB_FILE_NAME = "defensive.db"
#this function create the database tables
def init_db():
    conn = sqlite3.connect(DB_FILE_NAME)
    c = conn.cursor()
    try:
        conn.executescript("""
        CREATE TABLE IF NOT EXISTS clients (ID BLOB(16) PRIMARY KEY,
            Username TEXT NOT NULL UNIQUE,
            PublicKey BLOB(160) NOT NULL,
            LastSeen TEXT);
            
        CREATE TABLE IF NOT EXISTS messages (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            ToClient BLOB(16) NOT NULL,
            FromClient BLOB(16) NOT NULL,
            Type INTEGER NOT NULL,
            Content BLOB NOT NULL);""")

        conn.commit()

    except:
        print("tables are not created, possibly already exists")
