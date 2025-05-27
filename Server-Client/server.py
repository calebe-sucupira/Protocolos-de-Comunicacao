import socket
import threading
from datetime import datetime

HOST = '172.25.188.137'
PORT = 65432

clients = {}
lock = threading.Lock()

def broadcast(sender, message):
    timestamp = datetime.now().strftime('%d/%m/%Y %H:%M:%S')
    formatted = f"[{timestamp}] {sender}: {message}"
    
    with lock:
        for username, conn in clients.items():
            try:
                conn.sendall(formatted.encode())
            except:
                del clients[username]

def handle_client(conn, addr):
    username = None
    try:
        username = conn.recv(1024).decode().strip()
        with lock:
            clients[username] = conn
        
        print(f"{username} conectado!")
        broadcast("Servidor", f"{username} entrou no chat")
        
        while True:
            message = conn.recv(1024).decode().strip()
            if not message:
                break
            broadcast(username, message)
            
    except Exception as e:
        print(f"Erro: {e}")
    finally:
        if username:
            with lock:
                if username in clients:
                    del clients[username]
            broadcast("Servidor", f"{username} saiu do chat")
        conn.close()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen()
    print("Servidor aguardando conexões...")
    
    while True:
        conn, addr = s.accept()
        client_thread = threading.Thread(target=handle_client, args=(conn, addr))
        client_thread.start()
