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
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
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
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  int is_static; 
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio; 

  /* Read request line and headers */  
  Rio_readinitb(&rio, fd); 
  Rio_readlineb(&rio, buf, MAXLINE); 
  printf("Request headers:\n");
  printf("%s", buf); 
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("Get image file uri : %s\n", uri);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }


  //read_requesthdrs(&rio); // 이것 때문에 Server가 무한 반복함.

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) { 
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); 
    return;
  }

  if (is_static) { 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { 
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { 
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program"); 
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }  
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);
  
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  // 에러 메시지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* read_requesthdrs: 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시한다.(그냥 프린트)*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE); // Host: localhost:8000 => printf를 하지 않았기에 출력되지 않음

  /* 버퍼 rp의 마지막 끝을 만날 때까지 ("Content-length: %d\r\n\r\n에서 마지막 \r\n") */
  /* 계속 출력해줘서 없앤다. */
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  /* Static content */
  /* uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 리턴한다. */

  // 예시
  // Request Line: GET /godzilla.jpg HTTP/1.0
  // uri: /godzilla.jpg
  // cgiargs: x(없음)
  // filename: ./home.html

  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");              // cgiargs는 동적 컨텐츠의 실행 파일에 들어갈 인자이기에 정적 컨텐츠의 경우 없다.
    strcpy(filename, ".");            // 현재 디렉토리에서 시작
    strcat(filename, uri);            // uri 넣어줌

    // 만약 uri 뒤에 '/'이 있다면 그 뒤에 home.html을 붙인다.
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }


  /* Dynamic content */
  /* uri에 cgi-bin이 있다면, 즉 동적 컨텐츠를 요청한다면 0을 리턴한다. */

  // 예시
  // Request Line: GET /cgi-bin/adder?15000&213 HTTP/1.0
  // uri: /cgi-bin/adder?123&123
  // cgiargs: 123&123
  // filename: ./cgi-bin/adder

  else
  {
    /* index: 문자를 찾았으면 문자가 있는 위치 반환 */
    /* ptr: '?'의 위치 */
    ptr = index(uri, '?');

    if (ptr)
    { /* '?'가 있으면 cgiargs를 '?' 뒤 인자들과 값으로 채워주고 ?를 NULL로 만든다. */
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else /* '?'가 없으면 그냥 아무것도 안 넣어줌. */
      strcpy(cgiargs, "");
    strcpy(filename, ".");            // 현재 디렉토리에서 시작
    strcat(filename, uri);            // uri 넣어줌
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);                         // 파일 이름의 접미어 부분 검사 => 파일 타입 결정
  sprintf(buf, "HTTP/1.1 200 OK\r\n");                      // 응답 라인 작성
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);       // 응답 헤더 작성
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  /* 응답 라인과 헤더를 클라이언트에게 보냄 */
  Rio_writen(fd, buf, strlen(buf)); 
  printf("Response headers: \n");
  printf("%s", buf);

  // /* Send response body to client */
  // srcfd = Open(filename, O_RDONLY, 0);  // filename의 이름을 갖는 파일을 읽기 권한으로 불러온다.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리에 파일 내용을 동적할당한다.
  // Close(srcfd);                         // 소켓을 열어놓는 것은 "치명적"인 메모리 누수 발생시킴 ..
  // Rio_writen(fd, srcp, filesize);       // 해당 메모리에 있는 파일 내용들을 fd에 보낸다.(읽는다.)
  // Munmap(srcp, filesize);               // 할당된 메모리 공간을 해제한다.

  // Homework 11.9: 정적 컨텐츠 처리할 때 요청 파일 malloc, rio_readn, rio_writen 사용하여 연결 식별자에게 복사
  if (strcasecmp(method, "GET") == 0){
    srcfd = Open(filename, O_RDONLY, 0);  // filename의 이름을 갖는 파일을 읽기 권한으로 불러온다.
    srcp = (char *)malloc(filesize);
    rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    rio_writen(fd, srcp, filesize);
    free(srcp);
  }
}

/*get_filetype - Derive file type from filename */
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
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpg"); // homework 11.7 mpg_video
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4"); 
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{ 
  // fork(): 함수를 호출한 프로세스를 복사하는 기능
  // 부모 프로세스(원래 진행되던 프로세스), 자식 프로세스(복사된 프로세스)
  // Tiny는 자식 프로세스를 fork하고, CGI 프로그램을 자식에서 실행하여 동적 컨텐츠를 표준 출력으로 보냄
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) // fork() 자식 프로세스 생성됐으면 0을 반환 (성공)
  { 
    /* Return first part of HTTP response */
    // 환경변수를 cigarg로 바꿔주겠다 0: 기존 값 쓰겠다 . 1: cigargs 
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);   
    // old file descriptor, new file descriptor
    // 화면에 출력할 수 있게끔 띄워주겠다 .
    Dup2(fd, STDOUT_FILENO);              // Redirect stdout to clinet
    Execve(filename, emptylist, environ); // Run CGI program
  }
  Wait(NULL); // Parent waits for and reaps child
}