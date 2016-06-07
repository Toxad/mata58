//SERVIDOR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define BUFF_SIZE 				2000
#define MAX_USERS 				20
#define NAME_SIZE 				30
#define MAX_COMMAND_SIZE		7
#define MSG_HELP 				"\nComandos válidos:\nSEND <Mensagem>\nEnvia mensagem para todos os clientes conectados;\n-------------\nSENDTO <Destinatário> <Mensagem>\nManda mensagem para cliente especificado;\n-------------\nWHO\nExibe lista de clientes conectados;\n-------------\nHELP\nLista de comandos válidos;\n"
#define NOT_UNIQUE_NAME			"ERRO: Nome não unico, escolha outro e tente novamente.\nDesconectado.\n"
#define MSG_COMMAND_ERROR 		"ERRO: Comando desconhecido. Digite <HELP> para lista de comandos aceitos.\n"

int fdmax;
fd_set global_fds;
char clientes_ativos[FD_SETSIZE][NAME_SIZE];
int pipefd[2];

time_t rawtime;
struct tm * timeinfo;
struct addrinfo hints, *res, *p;
struct sockaddr_storage their_addr;
socklen_t addr_size;

pthread_mutex_t lock;
pthread_t t1_thread;

int sockfd;
struct sockaddr_in my_addr;

void outHandler() {		//Função para tratamento do fechamento do servidor
	unsigned int j;
	for(j = 0; j <= fdmax; j++) {
		if(FD_ISSET(j, &global_fds)) {
			close(j);
		}
	}
	printf("\n");
	close(sockfd);
	exit(EXIT_SUCCESS);
}
//Thread para gerenciar novas conexões
void *reception(void *arg) {
	char buf[BUFF_SIZE];
	unsigned int i;
	int new_fd;
	memset(buf, 0, sizeof(buf));
	FD_SET(pipefd[0], &global_fds);	//Adiciona canal do pipe ao set global de FDs
	while(1) {
		listen(sockfd, 5);			//Espera conexões no socket 
		addr_size = sizeof(their_addr);
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);	//Aceita nova conexão de usuário
		if (new_fd == -1)
			perror("ERRO: Falha na conexão");
		else {
			//Recebe nome do novo cliente a se conectar
			while(1) {
				recv(new_fd, buf, BUFF_SIZE, 0);
				if(strlen(buf) > 0) {
					break;
				}
			}
			//Tratamento de nomes de usuário repetidos
			int unico = 1;
			for(i = 0; i <= fdmax; i++) {
				if(FD_ISSET(i, &global_fds)) {
					if(!strcmp(buf, clientes_ativos[i])) {
						unico = 0;
						break;
					}
				}
			}
			const char* nome_nao_unico = NOT_UNIQUE_NAME;
			if(!unico) {
				send(new_fd, nome_nao_unico, strlen(nome_nao_unico), 0);
				close(new_fd);
			}
			//Caso conexão seja bem sucedida e nome seja único
			else {
				fcntl(new_fd, F_SETFL, O_NONBLOCK);
				if(new_fd > fdmax) fdmax = new_fd;
				pthread_mutex_lock(&lock);	//Semáforo Mutex para acesso à variável global
				FD_SET(new_fd, &global_fds);
				pthread_mutex_unlock(&lock);
				strcpy(clientes_ativos[new_fd], buf);
				time (&rawtime);
				timeinfo = localtime (&rawtime);
				fprintf(stdout, "%d:%d\tNOME:%s\tConectado.\n",timeinfo->tm_hour, timeinfo->tm_min, buf);
				memset(buf, 0, sizeof(buf));
				const char* msg = "TRUE";
				write(pipefd[1], msg, strlen(msg));
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if(argc < 2) {	//Argumentos insuficientes
		fprintf(stderr, "usage: server_chat <PORT>\n");
		exit(EXIT_FAILURE);
	}

	struct sigaction act;
	act.sa_handler = outHandler;
	sigaction(SIGINT, &act, NULL);
	pthread_mutex_init(&lock, NULL);

	FD_ZERO(&global_fds);
	if(pipe(pipefd)) {
		perror("ERRO: Erro no pipe");
		exit(EXIT_FAILURE);
	}

	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);	//Manipulação de FD do pipe

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	//Operações de inicialização do servidor através de getaddrinfo(), socket() e bind()
	{
		int server_status;
		if ((server_status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(server_status));
			exit(EXIT_FAILURE);
		}
		int yes=1;
		for(p = res; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
				perror("ERRO: Erro na conexão do socket");
				continue;
			}
			
				setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				perror("ERRO:Falha ao dar bind no socket");
				continue;
			}
			break; 
		}
		if (p == NULL) {
			//Não encontrou nenhum socket válido na lista
			perror("ERRO: Não foi possivel encontrar um socket disponível");
			exit(EXIT_FAILURE);
		}
		freeaddrinfo(res);	//Libera lista encadeada
	}

	fdmax = sockfd;
	pthread_create(&t1_thread, NULL, reception, NULL);	//Inicia thread para receber novas conexões após servidor ser iniciado com sucesso

	char buf[BUFF_SIZE];
	memset(buf, 0, sizeof(buf));
	unsigned int i, j;
	fd_set T2_fds, copy_fds;
	int nbytes;
	//Prepara sets de FDs para operação
	FD_ZERO(&T2_fds);
	FD_SET(pipefd[0], &T2_fds);

	//Loop Principal
	while (1) {
		copy_fds = T2_fds;
		//Identifica qual socket está pronto para atual
		if(select(fdmax+1, &copy_fds, NULL, NULL, NULL) == -1) {
			perror("ERRO: Erro no select");
			exit(EXIT_FAILURE);
		}
		else {
			for(i = 3; i <= fdmax; i++) {

				if(FD_ISSET(i, &T2_fds)) {
					if(i == pipefd[0]) {
						if((nbytes = read(i, buf, sizeof(buf))) > 0) {
							if(strcmp(buf, "TRUE") == 0) {
								pthread_mutex_lock(&lock);
								T2_fds = global_fds;	//Atualização de set local de FDs protegido por mutex
								pthread_mutex_unlock(&lock);
							}
							memset(buf, 0, sizeof(buf));
						}
					}
					else {
						nbytes = recv(i, buf, sizeof(buf), 0);
						if (nbytes  == 0) {		//Ocorrência de cliente desconectando do servidor
							time (&rawtime);
							timeinfo = localtime (&rawtime);
							fprintf(stdout, "%d:%d\tNOME:%s\tDesconetado.\n",timeinfo->tm_hour, timeinfo->tm_min, clientes_ativos[i]);
							close(i);
							FD_CLR(i, &T2_fds);
							pthread_mutex_lock(&lock);
							FD_CLR(i, &global_fds);		//Atualização de set local de FDs protegido por mutex
							pthread_mutex_unlock(&lock);
						}
						else if (nbytes > 0) {
							int valido = 0;
							j = 0;
							//Separa inicio da mensagem recebida para tratamento de comandos
							char command[MAX_COMMAND_SIZE];
							while(!isspace((int) buf[j]) && j++ <= strlen(buf));
							if(j < MAX_COMMAND_SIZE && strlen(buf)) {
								int separator = j;
								j = 0;
								memset(command, 0, sizeof(command));
								do { 
									command[j] = buf[j]; 
								} while(++j < separator);
								char* msg_inicio = &buf[separator+1];
								if(strlen(msg_inicio) < 0)
									continue;
								j = 0;
								//Converte entrada para caixa alta (comandos validos para letras maiusculas e minusculas)
								while(j < strlen(command)) { command[j] = toupper(command[j]); j++; }
								if(!strcmp(command, "SEND")) {		//Usuário enviou comando SEND
									valido = 1;
									char tmp[BUFF_SIZE] = {0};
									strcpy(buf, msg_inicio);
									if(strlen(buf) + strlen(clientes_ativos[i]) < BUFF_SIZE)
										strcat(strcat(strcat(tmp, clientes_ativos[i]), ": "), buf);
									else {
										strcat(strcat(tmp, clientes_ativos[i]), ": ");
										j = 0;
										const int nome_len = strlen(clientes_ativos[i]);
										while(j < strlen(buf) && j+nome_len+2 < BUFF_SIZE) {
											tmp[j+nome_len+2] = buf[j];
											j++;
										}
									}
									for(j = 0; j <= fdmax; j++) {
										if(FD_ISSET(j, &T2_fds) && i != j) {
											send(j, tmp, sizeof(tmp), 0);	//Envia mensagem para todos os clientes conectados
										}
									}
								}
								else if(!strcmp(command, "SENDTO")) {
									char nome[NAME_SIZE];
									j = separator+1;
									int aux = j;
									//Separa nome do destinatário de mensagem a enviar
									while(!isspace((int) buf[j]) && j++ <= strlen(buf));
									if(j < NAME_SIZE && strlen(buf)) {
										separator = j;
										j = 0;
										memset(nome, 0, sizeof(nome));
										do { 
											nome[j] = buf[j+aux];
										} while(++j < (separator - aux));
										msg_inicio = &buf[separator+1];
										if(strlen(msg_inicio) < 0)
										continue;
										char tmp[BUFF_SIZE] = {0};
										strcpy(buf, msg_inicio);
											if(strlen(buf) + strlen(clientes_ativos[i]) < BUFF_SIZE)
											strcat(strcat(strcat(tmp, clientes_ativos[i]), ": "), buf);
										else {
											strcat(strcat(tmp, clientes_ativos[i]), ": ");
											j = 0;
											const int nome_len = strlen(clientes_ativos[i]);
											while(j < strlen(buf) && j+nome_len+2 < BUFF_SIZE)
												tmp[j+nome_len+2] = buf[j];
										}

										for(j = 0; j <= fdmax; j++) {
											if(FD_ISSET(j, &T2_fds) && i != j) {
												if(!strcmp(clientes_ativos[j], nome)) {	//Envia mensagem apenas para cliente desejado
													valido = 1;
													send(j, tmp, sizeof(tmp), 0);
													break;
												}
											}
										}
									}

								}
								else if(!strcmp(command, "WHO")) {
									valido = 1;
									char msg_aux[NAME_SIZE + 7];
									strcpy(buf, "Clientes Ativos:\n-----------");
									send(i, buf, sizeof(buf), 0);
									for(j = 0; j<MAX_USERS; j++ ){
										if(FD_ISSET(j, &T2_fds) && j!=pipefd[0]){	//Percorre nome de todos usuários ativos e envia para cliente requisitante
											sprintf(msg_aux, "%s",clientes_ativos[j]);
											strcpy(buf, msg_aux);
											send(i, buf, sizeof(buf), 0);
										}
									}
								}
								else if(!strcmp(command, "HELP")) {
									const char *msg_help = MSG_HELP;
									valido = 1;
									strcpy(buf, msg_help);
									send(i, buf, sizeof(buf), 0);
								}
								else if(!strcmp(command, "TOP")) {
									//top
								}
								else {
									const char *msg_erro = MSG_COMMAND_ERROR;	//Envia mensagem para usuário em caso de comando desconhecido
									strcpy(buf, msg_erro);
									send(i, buf, sizeof(buf), 0);
								}

								fprintf(stdout, "%d:%d\t%s\t\"%s\"\tExecutado: %s.\n",timeinfo->tm_hour, timeinfo->tm_min, clientes_ativos[i], command, (valido) ? "Sim" : "Não");
								while(recv(i, buf, sizeof(buf), 0) > 0);
							}
							else {
									const char *msg_erro = MSG_COMMAND_ERROR;	//Envia mensagem para usuário em caso de comando desconhecido
									strcpy(buf, msg_erro);
									send(i, buf, sizeof(buf), 0);
							}
						}
						memset(buf, 0, sizeof(buf));
					}
				}
			}
		}
	}
	return 0;
}