/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1); // 실행했을 때 인자가 2개가 아니면 프로그램 종료
  }

  listenfd = Open_listenfd(argv[1]); // argv[1]에 있는 입력으로 받은 포트 번호를 인자로 주고 듣기 식별자를 반환 받는다.(내부적으로 getaddrinfo, socket, setsockopt, bind, listen 함수가 동작하여 연결 요청을 받을 준비가 된 듣기 식별자를 반환한다.)
  while (1) { // 클라이언트에게 지속적인 서비스를 제공하기 위한 무한루프
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, // 클라이언트로부터 연결 요청을 기다린다. clientaddr에 연결된 클라이언트의 주소가 저장됨, 연결을 위한 새로운 연결 식별자가 반환되어 connfd에 저장됨
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); // 클라이언트의 주소 정보 구조체를 클라이언트의 호스트 이름과 포트 번호로 변환해 각각 client_hostname, client_port에 저장
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 연결된 클라이언트의 정보(호스트 이름, 포트 번호) 출력
    doit(connfd);   // line:netp:tiny:doit, 트랜잭션 수행
    Close(connfd);  // line:netp:tiny:close, 연결을 닫음
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd); // rio_t 구조체 초기화, 인자로 받은 연결 소켓 식별자를 fd에 대입
  Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인을 읽어서 buf에 저장한다.
  printf("Request headers:\n");
  printf("%s", buf); // buf에 저장된 요청 라인 출력
  sscanf(buf, "%s %s %s", method, uri, version); // buf에 저장된 요청 라인을 3개의 문자열로 읽어와 method, uri, version 각각의 변수에 저장
  if (strcasecmp(method, "GET")) { // method와 "GET"을 대소문자 구분없이 비교하는 함수의 반환값이 0이 아니면(같지 않다면)
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method"); // Tiny는 GET 메소드만 지원하므로 다른 메소드를 요청하면 에러 메시지 출력
    return; // main으로 돌아감
  }
  read_requesthdrs(&rio); // 헤더의 내용을 읽어들임

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // uri의 구조를 보고 정적 컨텐츠를 요청하면 1을 동적 컨텐츠를 요청하면 0을 반환하는 함수 parse_uri를 호출하고 반환값을 is_static에 저장
  if (stat(filename, &sbuf) < 0) { // stat함수가 파일 정보를 읽어오는데 실패했다면(파일의 정보를 읽어 오는 함수 stat은 성공 시 0을 반환하며 sbuf에 파일 정보를 채운다. 실패하면 -1을 반환한다.)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); // 클라이언트에게 오류에 대해 보고하는 함수 clienterror에 오류에 관한 정보를 넘기면서 호출, 파일이 디스크 상에 없음을 알림
    return;
  }

  if (is_static) { /* Serve static content */ // 클라이언트가 정적 컨텐츠를 요청했다면(is_static이 1)
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 파일이 보통 파일이 아니거나 읽기 권한이 없다면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file"); // 403(액세스 금지) 오류를 clienterror에 넘기면서 호출
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // 정적 컨텐츠를 제공
  } else { /* Serve dynamic content*/ // 클라이언트가 동적 컨텐츠를 요청했다면(is_static이 1아님)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 파일이 보통 파일이 아니거나 읽기 권한이 없다면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program"); // 403(액세스 금지) 오류를 clienterror에 넘기면서 호출
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 컨텐츠를 제공
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>"); // HTML 문서의 여는 태그와 문서의 title를 body 버퍼에 작성
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); // 배경이 흰색(#ffffff)인 body 태그를 body 버퍼에 추가
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); // errnum(에러 번호)과 shortmsg(짧은 메시지)를 body 버퍼에 추가
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); // longmsg(긴 메시지)와 cause(에러 원인)를 단락안에 넣어서 body 버퍼에 추가
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 수평선과 서버 이름을 이탤릭체로 body 버퍼에 추가

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 응답 라인을 buf에 작성
  Rio_writen(fd, buf, strlen(buf)); // 응답 라인을 클라이언트에게 보내기 위해 연결 소켓에 씀
  sprintf(buf, "Content-type: text/html\r\n"); // 응답 본체 내 컨텐츠의 MIME 타입 text/html을 알려주는 Content-type 헤더를 buf에 출력
  Rio_writen(fd, buf, strlen(buf)); // Content-type 헤더를 클라이언트에게 보내기 위해 연결 소켓에 씀
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 컨텐츠의 크기를 알려주는 Content-length 헤더를 buf에 출력, 헤더의 끝을 알리는 "\r\n"을 뒤에 붙임
  Rio_writen(fd, buf, strlen(buf)); // Content-length 헤더와 빈 줄을 클라이언트에게 보내기 위해 연결 소켓에 씀
  Rio_writen(fd, body, strlen(body)); // 응답 본체를 클라이언트에게 보내기 위해 연결 소켓에 씀
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // // \n이나 EOF에 도달할 때까지 최대 MAXLINE - 1개의 문자를 읽어서 buf에 저장한다.(한 줄씩 읽음)
  while(strcmp(buf, "\r\n")) { // 읽어들인 데이터와 "\r\n"을 비교하는 함수의 반환값이 0이 아닌 동안 반복(입력이 "\r\n"이 아닌 동안 반복, "\r\n"은 요청 헤더를 종료한다는 의미)
    Rio_readlineb(rp, buf, MAXLINE); // 반복해서 한 줄씩 입력 받는다.
    printf("%s", buf); // buf에 있는 내용을 출력
  }
  return; 
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { /* Static content */ // uri안에 "cgi-bin"이 존재하지 않는다면(정적 컨텐츠)
    strcpy(cgiargs, ""); // CGI 인자 문자열을 빈 문자열로 초기화
    strcpy(filename, "."); // filename을 현재 디렉토리를 의미하는 "."으로 초기화
    strcat(filename, uri); // uri에 filename을 이어붙여서 상대 리눅스 경로 이름 생성
    if (uri[strlen(uri)-1] == '/') // uri의 마지막 문자가 '/'라면
      strcat(filename, "home.html"); // 기본 파일 이름("home.html")을 추가
    return 1; // 1을 반환(정적 컨텐츠임)
  } else { /* Dynamic content */ // uri안에 "cgi-bin"이 존재한다면(동적 컨텐츠)
    ptr = index(uri, '?'); // uri에서 '?'를 찾아 그 위치를 ptr에 기록(없다면 NULL)
    if (ptr) { // uri 안에 '?'이 있다면 (ptr이 NULL이 아니면)
      strcpy(cgiargs, ptr+1); // cgiargs에 uri의 '?' 다음 문자열을 복사
      *ptr = '\0'; // '?' 위치에는 null 문자를 넣어서 uri 문자열을 자름, cgi 스크립트의 경로와 스크립트에 전달할 매개변수를 구분하기 위해
    }
    else // uri에 '?'이 없다면
      strcpy(cgiargs, ""); // CGI 인자 문자열을 빈 문자열로 초기화
    strcpy(filename, "."); // filename을 현재 디렉토리를 나타내는 "."으로 초기화
    strcat(filename, uri); // uri를 filename에 이어붙여 상대 리눅스 경로 이름 생성
    return 0; // 0을 반환(동적 컨텐츠임)
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXLINE];

  /* Send response headers to client */
  get_filetype(filename, filetype); // 파일 이름의 접미어를 검사해서 파일 타입을 filetype에 반환하는 get_filetype 함수 호출
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 응답 라인을 buf에 작성 - 200
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // Server 헤더를 buf에 작성
  sprintf(buf, "%sConnection: close\r\n", buf); // Connection 헤더를 buf에 작성
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize); // Content-length 헤더를 buf에 작성
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // Content-type 헤더를 buf에 작성, 헤더의 끝을 알리는 "\r\n"을 뒤에 붙임
  Rio_writen(fd, buf, strlen(buf)); // buf의 응답 라인과 헤더들을 클라이언트에게 보내기 위해 연결 소켓에 씀
  printf("Response headers:\n"); // 응답 헤더에 대해 출력
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // 요청된 파일을 읽기 전용으로 열고 srcfd에 파일의 파일 디스크립터를 저장
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 요청된 파일을 가상메모리 영역에 매핑하고 그 시작 주소를 srcp에 저장
  Close(srcfd); // 메모리 누수 방지를 위해 파일 디스크립터를 닫음, 파일을 메모리에 매핑했기 때문에 파일 디스크립터를 닫아도 됨
  Rio_writen(fd, srcp, filesize); // 메모리에 매핑된 파일 내용을 클라이언트에게 보내기 위해 연결 소켓에 씀 (srcp에서 시작하는 filesize 바이트를 연결 식별자로 복사)
  Munmap(srcp, filesize); // 메모리 누수를 방지하기 위해 메모리 매핑을 해제(매핑된 가상 메모리 주소를 반환)
}

/* 
 * get_filetype - Derive file type from filename
 * 파일 이름의 접미어 부분을 검사해서 파일 타입을 결정하고 filetype에 기록하는 함수
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 응답 라인을 buf에 작성 - 200
  Rio_writen(fd, buf, strlen(buf)); // buf의 응답 라인을 클라이언트에게 보내기 위해 연결 소켓에 씀
  sprintf(buf, "Server: Tiny Web Server\r\n"); // Server 헤더를 buf에 작성
  Rio_writen(fd, buf, strlen(buf)); // buf의 Server 헤더를 클라이언트에게 보내기 위해 연결 소켓에 씀

  if (Fork() == 0) { /* Child */ // Fork() 함수로 새로운 자식 프로세스를 생성하고 자식 프로세스에서 CGI 프로그램을 실행하며 모든 종류의 동적 컨텐츠를 제공
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // QUERY_STRING 환경변수를 요청 URI의 CGI 인자들로 초기화
    Dup2(fd, STDOUT_FILENO); /*Redirect stdout to client */ // CGI 프로그램의 출력이 클라이언트에게 전송될 수 있도록 자식 프로세스의 표준 출력을 클라이언트의 연결 식별자로 리다이렉션
    Execve(filename, emptylist, environ); /* Run CGI program */ // CGI 프로그램을 실행, 현제 프로세스의 메모리 이미지를 CGI 프로그램으로 대체
  }
  Wait(NULL); /* Parent waits for and reaps child */ // 부모 프로세스는 자식 프로세스가 종료될 때까지 대기
}