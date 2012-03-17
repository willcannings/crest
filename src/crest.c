#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include "crest.h"

#define MAX_LINE_LENGTH 10 * 1024

/*------------------------------------------------------------*/
/* private functions                                          */
/*------------------------------------------------------------*/
int crest_read_line(crest_connection *connection) {
  int bytes = 0, total = 0, crlf_found = 0;
  
  // check if the current buffer contains a new line
  char *ptr = connection->request_data;
  char *end = connection->request_buffer + (data_length - 1);
  
  while(ptr < end) {
    if((ptr[0] == CR) && (ptr[1] == LF))
      return CREST_READ_OK;
    ptr++;
  }
  
  // read more from the socket if a line isn't present
  // TODO: handle realloc failure
  if((connection->request_buffer_size - connection->request_data_size) < MAX_LINE_LENGTH)
    connection->request_data = (char *) realloc(connection->request_data, connection->request_buffer_size + MAX_LINE_LENGTH);
  char *buffer = connection->request_data + connection->request_data_size;
    
  while(bytes < MAX_LINE_LENGTH && !crlf_found) {
    bytes = read(connection->client, buffer, MAX_LINE_LENGTH - total);
    total += bytes;
    
    // -1 == error, 0 = EOF
    if(bytes < 1)
      return CREST_READ_ERROR;
    
    // check for CRLF in new data
    // TODO: should only check newly read range rather than the entire
    // range of bytes read over all iterations. check for edge case
    // where last byte of previous read was CR.
    for(int i = 0; i < (total - 1); i++) {
      if((buffer[i] == CR) && (buffer[i + 1] == LF)) {
        crlf_found = 1;
        break;
      }
    }
  }
  
  connection->request_data_size += total;
  return CREST_READ_OK;
}

int crest_parse_request_line(crest_connection *connection) {
	char *end, *start;
  assert(connection);
	
  switch(((uint32_t *)connection->request_data)[0]) {
    case 'GET ':
      connection->method = http_get;
      connection->request_data += 3;
      break;
    
    case 'POST':
      connection->method = http_get;
      connection->request_data += 4;
      break;
    
    case 'PUT ':
      connection->method = http_post;
      connection->request_data += 3;
      break;
    
    case 'PATC':
      connection->method = http_patch;
      connection->request_data += 5;
      break;
    
    case 'DELE':
      connection->method = http_delete;
      connection->request_data += 6;
      break;
    
    case 'HEAD':
      connection->method = http_head;
      connection->request_data += 4;
      break;
    
    case 'OPTI':
      connection->method = http_options;
      connection->request_data += 7;
      break;
    
    default:
      // TODO: respond with unknown method
      return CREST_PARSE_ERROR;
  }
  
  // ensure we matched a method name correctly, by checking if
  // there's a space where we expect it
  // TODO: respond with unknown method
  if(*connection->request_data != ' ')
    return CREST_PARSE_ERROR;

  // TODO: honour MAX_URI_LENGTH
  connection->uri = ++(connection->request_data);
	move_to_end_of_uri(connection->request_data);
	
	// TODO: respond with unknown method
	if(*connection->request_data != ' ')
    return CREST_PARSE_ERROR;
  *connection->request_data = 0;
  (connection->request_data)++;

	// match 'HTTP/'
	if(strstr(connection->request_data, HTTP_VERSION_PREFIX) == start)
		start += HTTP_VERSION_PREFIX_LEN;
	else
		return CREST_PARSE_ERROR;
	
	// major version number
	end = start;
	move_to_end_of_digits(connection->request_data);
	if((end-start) > MAX_VERSION_LENGTH || (end-start) == 0)
		return CREST_PARSE_ERROR;
	connection->http_major_version = (unsigned long) strtol(start, &(end), 10);
	
	// minor version number
	start = ++end;
	move_to_end_of_digits(connection->request_data);
	if((end-start) > MAX_VERSION_LENGTH || (end-start) == 0)
		return CREST_PARSE_ERROR;
	connection->http_minor_version = (unsigned long) strtol(start, &(end), 10);
	
	// check to make sure we're at the end of the line
	if((*end == CR) && (*(++end) == LF))
		return CREST_PARSE_OK;
	else
		return CREST_PARSE_ERROR;	
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
  memset(&connection, 0, sizeof(crest_connection));
  
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
    client = accept(server, (struct sockaddr *)&clientaddr, &clientaddr_len);
    if(client == -1)
      continue;
    
    // cleanup old connection
    // TODO: free connection address once it's being stored
    if(connection.request_header_keys_count > 0) {
      for(int i = 0; i < connection.request_header_keys_count; i++) {
        free(connection.request_header_keys[i]);
        free(connection.request_header_values[i]);
      }
    }

    if(connection.response_header_keys_count > 0) {
      for(int i = 0; i < connection.request_header_keys_count; i++) {
        free(connection.response_header_keys[i]);
        free(connection.response_header_values[i]);
      }
    }

    if(connection.request_data)
      free(connection.request_data);

    if(connection.response_body)
      free(connection.response_body);
    
    // initialise new connection
    memset(&connection, 0, sizeof(crest_connection));
    connection.server = server;
    connection.client = client;
    
    // TODO: handle read error
    if(crest_read_line(&connection) != CREST_READ_OK)
      return;
    
    // TODO: handle parse error - 400
    if(crest_parse_request_line(&connection) != CREST_PARSE_OK)
      return;
  }
}

void crest_write_string(crest_connection *connection, char *data) {
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

