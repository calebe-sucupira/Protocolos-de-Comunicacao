import socket
import threading

HOST = '127.0.0.1'
PORT = 12345

CITIES = ["Quixadá", "Fortaleza", "Recife", "São Paulo", "Rio de Janeiro"]
TIPOS_VALIDOS = ['temperatura', 'umidade', 'descricao', 'previsao']
slaves = {}

def handle_slave(conn, addr):
    try:
        slave_type = conn.recv(1024).decode().strip()
        if slave_type not in TIPOS_VALIDOS:
            print(f"[RECUSADO] Tipo inválido: {slave_type}")
            conn.close()
            return
        slaves[slave_type] = conn
        print(f"[REGISTRO] Escravo {slave_type} conectado em {addr}")

        while True:
            pass  # mantém a conexão viva

    except Exception as e:
        print(f"[ERRO] Escravo {slave_type} desconectado: {e}")
        if slave_type in slaves:
            del slaves[slave_type]
        conn.close()

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print("\n[MESTRE] Aguardando escravos na porta 12345...")
        while True:
            conn, addr = s.accept()
            print(f"\n[CONEXÃO] Nova conexão de {addr}")
            threading.Thread(target=handle_slave, args=(conn, addr)).start()

def display_menu():
    print("\n=== Cidades Disponíveis ===")
    for idx, city in enumerate(CITIES, 1):
        print(f"{idx}. {city}")
    print("===========================")

threading.Thread(target=start_server, daemon=True).start()

while True:
    display_menu()
    try:
        tipos = input("\nTipos de dado (ex: temperatura umidade previsao descricao ou 'todos'): ").strip().lower().split()
        if 'todos' in tipos:
            tipos = TIPOS_VALIDOS

        cidade_idx = int(input("Número da cidade: ")) - 1
        if cidade_idx < 0 or cidade_idx >= len(CITIES):
            print("Erro: Número inválido!")
            continue

        print(f"\n[ENVIO] Solicitando {', '.join(tipos)} para {CITIES[cidade_idx]}")

        for tipo in tipos:
            if tipo not in slaves:
                print(f"[FALHA] Escravo '{tipo}' não disponível.")
                continue

            try:
                slaves[tipo].sendall(str(cidade_idx).encode())
                resposta = slaves[tipo].recv(1024).decode()
                print(f"[{tipo.upper()}] {resposta}")
            except Exception as e:
                print(f"[ERRO] ao enviar para {tipo}: {e}")

    except ValueError:
        print("Erro: Insira um número válido!")
    except Exception as e:
        print(f"Erro: {e}")
