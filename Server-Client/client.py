import socket
import threading
import sys
import time
from datetime import datetime

HOST = '172.25.188.83'  # substitua pelo IP do servidor
PORT = 65432

COLORS = ['\033[94m', '\033[92m', '\033[93m', '\033[95m']
RESET = '\033[0m'

def get_color(username):
    return COLORS[hash(username) % len(COLORS)]

def format_message(username, message):
    timestamp = datetime.now().strftime('%H:%M')
    color = get_color(username)
    header = f"{color}╭─ {username} ({timestamp}){RESET}"
    body = f"{color}│{RESET} {message}"
    footer = f"{color}╰─>{RESET}"
    return f"\n{header}\n{body}\n{footer}"

def clear_input_line():
    sys.stdout.write('\033[2K\033[1G')  # limpa a linha atual
    sys.stdout.flush()

def receive_messages(sock, username, connected_event):
    try:
        while connected_event.is_set():
            message = sock.recv(1024).decode()
            if not message:
                raise ConnectionError
            clear_input_line()
            print(message)
            sys.stdout.write(f"{get_color(username)}Você: {RESET}")
            sys.stdout.flush()
    except:
        connected_event.clear()

def connect_to_server(username):
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((HOST, PORT))
            sock.sendall(username.encode())
            print("[✓] Conectado ao servidor.")
            return sock
        except Exception:
            print("[...] Servidor indisponível. Tentando novamente em 5 segundos...")
            time.sleep(5)

def start_client():
    username = input("Digite seu nome de usuário: ")

    connected_event = threading.Event()

    while True:
        sock = connect_to_server(username)
        connected_event.set()

        receiver = threading.Thread(target=receive_messages, args=(sock, username, connected_event))
        receiver.daemon = True
        receiver.start()

        print("\n=== Chat em Tempo Real (digite '/sair' para encerrar) ===")

        while connected_event.is_set():
            try:
                message = input(f"{get_color(username)}Você: {RESET}")
                if message.lower() == '/sair':
                    sock.close()
                    print("\n[✓] Desconectado.")
                    return
                formatted = format_message(username, message)
                sock.sendall(formatted.encode())
            except (ConnectionResetError, BrokenPipeError):
                print("\n[!] Conexão perdida. Tentando reconectar...")
                connected_event.clear()
                break
            except KeyboardInterrupt:
                sock.close()
                return

        sock.close()
        print("[!] Reconectando...")

if __name__ == "__main__":
    start_client()

