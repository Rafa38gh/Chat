# Chat

#Atenção, é necessário compilar o código usando Linux.

1. Conecte-se a máquina virtual:
```sh
ssh nome@ip -p porta
```
Um exemplo prático seria: ssh rafael@127.0.0.0 -p 3333

2. Monte a pasta compartilhada
```sh
sudo mkdir /mnt/chat
```
```sh
sudo mount chat /mnt/chat
```

3. Acesse a pasta compartilhada
```sh
cd /mnt/chat
```

4. Compile o servidor e o cliente
```sh
gcc server.c -o server
```
```sh
gcc client.c -o client
```

5. Em dois terminais diferentes, execute o cliente e o servidor
```sh
./server
```
```sh
./client
```