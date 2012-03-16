#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include "crest.h"

/*------------------------------------------------------------*/
/* private functions                                          */
/*------------------------------------------------------------*/
int crest_parse_request_line(crest_connection *connection, char *data, char **data_end) {
	char *end, *start;

	// pre-conditions
	assert(data);
	assert(data_end);
	assert(connection);
	
  switch(((uint32_t *)data)[0]) {
    case 'GET ':
      connection->method = http_get;
      data += 3;
      break;
    
    case 'POST':
      connection->method = http_get;
      data += 4;
      break;
    
    case 'PUT ':
      connection->method = http_post;
      data += 3;
      break;
    
    case 'PATC':
      connection->method = http_patch;
      data += 5;
      break;
    
    case 'DELE':
      connection->method = http_delete;
      data += 6;
      break;
    
    case 'HEAD':
      connection->method = http_head;
      data += 4;
      break;
    
    case 'OPTI':
      connection->method = http_options;
      data += 7;
      break;
    
    default:
      // TODO: respond with unknown method
      return CREST_PARSE_ERROR;
  }
  
  // ensure we matched a method name correctly, by checking if
  // there's a space where we expect it
  // TODO: respond with unknown method
  if(*data != ' ')
    return CREST_PARSE_ERROR;

  // TODO: honour MAX_URI_LENGTH
  connection->uri = ++data;
	move_to_end_of_uri(data);
	
	// TODO: respond with unknown method
	if(*data != ' ')
    return CREST_PARSE_ERROR;
  *data = 0;
  data++;

	// match 'HTTP/'
	if(strstr(data, HTTP_VERSION_PREFIX) == start)
		start += HTTP_VERSION_PREFIX_LEN;
	else
		return CREST_PARSE_ERROR;
	
	// major version number
	end = start;
	move_to_end_of_digits(end, data_end);
	if((end-start) > MAX_VERSION_LENGTH || (end-start) == 0)
		return CREST_PARSE_ERROR;
	connection->major_version = (unsigned long) strtol(start, &(end), 10);
	
	// minor version number
	start = ++end;
	move_to_end_of_digits(end, data_end);
	if((end-start) > MAX_VERSION_LENGTH || (end-start) == 0)
		return CREST_PARSE_ERROR;
	connection->minor_version = (unsigned long) strtol(start, &(end), 10);
	
	// check to make sure we're at the end of the line
	if(*end == CR)
		return CREST_PARSE_OK;
	else
		return CREST_PARSE_ERROR;
	
  *data_end = data;
}


/*------------------------------------------------------------*/
/* server functions                                           */
/*------------------------------------------------------------*/
void crest_start_server(int port) {
  struct sockaddr_in servaddr, clientaddr;
  int error = 0, server, client;
  socklen_t clientaddr_len;
  crest_connection connection;
  
  memset(&servaddr, 0, sizeof(servaddr));
  clientaddr_len = sizeof(clientaddr);
  
  // TODO: handle socket error
  server = socket(AF_INET, SOCK_STREAM, 0);
  if(server == -1)
    return;
  
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(port);
  
  // TODO: handle bind error
  error = bind(server, (const struct sockaddr *)&servaddr, sizeof(servaddr));
  if(error)
    return;
  
  // TODO: handle listen error
  error = listen(server, 1000);
  if(error)
    return;
  
  while(1) {
    client = accept(server, &clientaddr, &clientaddr_len);
    if(client == -1)
      continue;
      
    memset(&connection, 0, sizeof(crest_connection));
  }
}

void crest_write(crest_connection *connection, char *data) {
  crest_write(connection, data, strlen(data));
}

// TODO: use chained buffers to reduce allocations
void crest_write(crest_connection *connection, void *data, int length) {
  int old_length = connection->response_length;
  connection->response_length += length;
  connection->response_body = realloc(connection->response_body, connection->response_length);
  memcpy(connection->response_body + old_length, data, length);
}

void crest_complete(crest_connection *connection) {
  
}

