#include <stdio.h>
#include "crest.h"

void route_1(crest_connection *connection) {
  printf("Route 1\n");
}

void route_2(crest_connection *connection) {
  printf("Route 2\n");
}

void route_3(crest_connection *connection) {
  printf("Route 3\n");
}

int main(void) {
  crest_start_server(8080);
  return 0;
}
