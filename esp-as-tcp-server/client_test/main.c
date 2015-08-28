/* Sample TCP client */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

const char *str="Thank! you";

int main(int argc, char**argv)
{
	int sockfd,n;
	struct sockaddr_in servaddr,cliaddr;
	char recvline[1000];

	if (argc != 2)
	{
		printf("usage:  client <IP address>\n");
		exit(1);
	}

	sockfd=socket(AF_INET,SOCK_STREAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(argv[1]);
	servaddr.sin_port=htons(7777);

	connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	do 
	{
		n = recvfrom(sockfd, recvline, 10000, 0, NULL, NULL);
		if (n> 0)
		{
			recvline[n] = 0;
			fprintf(stderr, "%s [%d]\n", recvline, n);	
			send(sockfd,str, strlen(str), 0);
		}
	} while (n> 0);
}
