#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd; // 클라이언트의 소켓 식별자
    char *host, *port, buf[MAXLINE]; // 서버 호스트 이름, 서버 포트 번호, 버퍼
    rio_t rio; // robust I/O 구조체

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0); // 실행했을 때 인자가 3개가 아니면 프로그램 종료
    }
    host = argv[1]; // argv[1]는 호스트 이름
    port = argv[2]; // argv[2]는 포트 번호

    clientfd = Open_clientfd(host, port); // 서버와 연결 설정- 내부적으로 getaddrinfo, socket, connect 함수가 실행되며 host라는 서버의 port 포트 번호에 연결 요청을 설정하고 열린 소켓 식별자를 반환한다.
    Rio_readinitb(&rio, clientfd); // rio_t 구조체를 초기화, 파일 디스크립터에 소켓 식별자 대입

    while (Fgets(buf, MAXLINE, stdin) != NULL) { // (MAXLINE - 1)개의 문자를 입력받거나 \n, EOF에 도달할 때까지 사용자의 입력을 받아서 buf에 저장
        Rio_writen(clientfd, buf, strlen(buf)); // buf 크기만큼 소켓에 써서 서버로 보냄
        Rio_readlineb(&rio, buf, MAXLINE); // rio 구조체의 소켓에서 \n, EOF에 도달할 때까지 서버의 응답을 최대 MAXLINE - 1바이트를 읽어서 buf에 저장
        Fputs(buf, stdout); // buf에 저장된 서버의 응답을 화면에 출력
    }
    Close(clientfd); // 소켓 닫음
    exit(0);
}