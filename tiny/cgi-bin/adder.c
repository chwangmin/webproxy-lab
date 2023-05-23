/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    char *delim = "&";
    char *arg = strtok(buf, delim);
    strcpy(arg1, arg);
    arg = strtok(NULL, delim);
    strcpy(arg2, arg);

    char *delimiter = "=";
    char *token = strtok(arg1, delimiter);
    n1 = atoi(strtok(NULL, delimiter));
    *token = strtok(arg2, delimiter);
    n2 = atoi(strtok(NULL, delimiter));
  }


  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
         content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Connection-length: %d\r\n", (int)strlen(content));
  printf("Connection-type: text/html\r\n\r\n");

  if (strcasecmp((buf = getenv("REQUEST_METHOD")), "GET") == 0)
    printf("%s", content);
  
  fflush(stdout);

  exit(0);
}
