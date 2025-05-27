
import socket
import requests
import time

API_KEY = '70e3f671b76fc0033f21733c706fa4de'
HOST = '172.25.191.89'
PORT = 12345
CITIES = ["Quixadá", "Fortaleza", "Recife", "São Paulo", "Rio de Janeiro"]
slave_type = 'umidade'

def fetch_data(city_idx):
    try:
        city = CITIES[city_idx] + ",BR"
        url = f'https://api.openweathermap.org/data/2.5/weather?q={city}&appid={API_KEY}&units=metric&lang=pt_br'
        response = requests.get(url)
        if response.status_code != 200:
            return f"Erro API: Código {response.status_code}"
        data = response.json()
        return f'💧 Umidade em {CITIES[city_idx]}: {data["main"]["humidity"]}%'
    except Exception as e:
        return f"Erro: {str(e)}"

def conectar_ao_mestre():
    while True:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((HOST, PORT))
            s.sendall(slave_type.encode())
            print(f"[REGISTRO] Registrado como {slave_type}!")
            return s
        except Exception as e:
            print(f"[RECONEXÃO] Falha ao conectar: {e}. Tentando novamente em 5 segundos...")
            time.sleep(5)

def main():
    while True:
        s = conectar_ao_mestre()
        try:
            while True:
                cidade_idx = s.recv(1024).decode()
                if not cidade_idx:
                    print("[DESCONECTADO] Conexão encerrada pelo mestre.")
                    break
                idx = int(cidade_idx)
                print(f"[TAREFA] Cidade: {CITIES[idx]}")
                resposta = fetch_data(idx)
                s.sendall(resposta.encode())
                print("[ENVIO] Dados enviados")
        except Exception as e:
            print(f"[ERRO] Conexão perdida: {e}")
        finally:
            s.close()
            print("[INFO] Reconectando ao mestre em 5 segundos...")
            time.sleep(5)

if __name__ == "__main__":
    main()
