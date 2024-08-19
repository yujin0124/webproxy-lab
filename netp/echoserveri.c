#include "csapp.h"

/*
 * echo - 클라이언트로부터 받은 데이터를 다시 그대로 클라이언트에게 보내는 echo 서비스를 제공하는 함수
 */
void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd); // rio_t 구조체 초기화, 파일 디스크립터에 연결 식별자 대입
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { // EOF에 도달하거나 오류가 발생할 때까지 클라이언트로부터 데이터를 한 줄씩, 최대 MAXLINE - 1 바이트를 읽어서 buf에 저장, 읽은 바이트 수가 반환되어 n에 저장된다.
        printf("server received %d bytes\n", (int)n); // 읽은 바이트 수를 출력
        Rio_writen(connfd, buf, n); // 연결 소켓에 buf에서 n바이트만큼 써서 클라이언트로 보냄
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) { 
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0); // 실행했을 때 인자가 2개가 아니면 프로그램 종료
    }

    listenfd = Open_listenfd(argv[1]); // argv[1]에 있는 입력으로 받은 포트 번호를 인자로 주고 듣기 식별자를 반환 받는다.(내부적으로 getaddrinfo, socket, setsockopt, bind, listen 함수가 동작하여 연결 요청을 받을 준비가 된 듣기 식별자를 반환한다.)
    while (1) { // 클라이언트에게 지속적인 서비스를 제공하기 위한 무한루프
        clientlen = sizeof(struct sockaddr_storage); // 모든 종류의 구조체를 담을 수 있을 만큼 큰 sockaddr_storage 구조체의 크기를 저장
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트로부터 연결 요청을 기다린다. clientaddr에 연결된 클라이언트의 주소가 저장됨, 연결을 위한 새로운 연결 식별자가 반환되어 connfd에 저장됨
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); // 클라이언트의 주소 정보 구조체를 클라이언트의 호스트 이름과 포트 번호로 변환해 각각 cleint_hostname, client_port에 저장
        printf("Connected to (%s, %s)\n", client_hostname, client_port); // 연결된 클라이언트의 정보(호스트 이름, 포트 번호) 출력
        echo(connfd); // ehco 서비스 제공
        Close(connfd); // 연결 식별자 닫음
    }
    exit(0);
}