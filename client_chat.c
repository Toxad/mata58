//CLIENTE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <pthread.h>
#include <signal.h>

#define BUFF_SIZE 2000	//Tamanho desejado de buffer
#define NAME_SIZE 30	//Tamanho máximo de nome do usuário

struct addrinfo hints, *res, *p;
int sockfd;
char nome[NAME_SIZE];
char buf[BUFF_SIZE];
int len_name;
int nbytes, i;

fd_set readfds, copy_fds;

void outHandler() {		//Função para definir comportamento quando o servidor se desconecta
	printf("\n");
	close(sockfd);
	exit(0);
}

int main(int argc, char *argv[]) {
	if(argc < 4) {		//Número de argumentos passados insuficiente
		fprintf(stderr, "usage: server_chat <CLIENT_NAME> <SERVER_ADDRESS> <SERVER_PORT>\n");
		exit(EXIT_FAILURE);
	}

	{
		strcpy(nome, argv[1]);		//Recupera nome do usuário passado como argumento
		len_name = sizeof argv[1];	//Salva tamanho no nome


		struct sigaction act;
		act.sa_handler = outHandler;
		sigaction(SIGINT, &act, NULL);

		memset(&hints, 0, sizeof hints); //Preparação de parametros na estrutura a ser usada em getaddressinfo()
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM; 

		int status; 
		if ((status = getaddrinfo(argv[2], argv[3], &hints, &res)) != 0) {
			fprintf(stderr, "ERRO: getaddrinfo: %s\n", gai_strerror(status));
			return 2;
		}
		//Abertura de socket e operação connect. Loop até encontrar uma entrada válida
		for(p = res; p!= NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
				perror("ERRO: Problema na abertura de socket");
				continue;
			}
			if((connect(sockfd, p->ai_addr, p->ai_addrlen)) != 0){
				perror("ERRO: Falha na conexão");
				close(sockfd);
				continue;
			}
			//Ao contrário de uma aplicação de servidor, não é necessário dar bind() no socket para o cliente
			break;
		}
		if (p == NULL) {
			//Saiu do loop sem encontrar nenhuma entrada válida
			perror("ERRO: Falha na conexão");
			exit(2);
		}
		else {
			fprintf(stdout, "Conectado com sucesso!\n");
		}
		freeaddrinfo(res); //Libera memoria da lista encadeada de addrinfo
	}
	//Envia o nome do usuário para o servidor
	send(sockfd, nome, len_name, 0);

	FD_ZERO(&readfds);	//Prepara os sets de FDs para operação no loop principal
	FD_ZERO(&copy_fds);

	FD_SET(sockfd, &readfds);
	FD_SET(fileno(stdin), &readfds);

	while(1) {	//Loop principal
		copy_fds = readfds;
		if(select(sockfd+1, &copy_fds, NULL, NULL, NULL) > 0) {
			//Algum socket da lista esta pronto para operar
			memset(buf, 0, sizeof(buf)); 
			if(FD_ISSET(sockfd, &copy_fds)) {
				//Recebimento de argumentos
				if((nbytes = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
					if(strlen(buf) > 0) {
						fprintf(stdout, "%s\n", buf);
					}
				}
				else if(nbytes == 0) {	//Caso o servidor se desconecte
					fprintf(stdout, "Desconectado.\n");
					outHandler();
				}
			}
			else if(FD_ISSET(fileno(stdin), &copy_fds)) {
				//Envio
				char c;
				i = 0;
				while(read(fileno(stdin), (void*) &c, sizeof(c)) > 0 && c != '\n' && i < BUFF_SIZE-1)
					buf[i++] = c;
				send(sockfd, buf, sizeof(buf), 0);
			}
		}
		else {
		}
	}
	return 0;
}



