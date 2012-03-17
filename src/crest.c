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

#define MAX_LINE_LENGTH   10 * 1024
#define MAX_BUFFER_READ   10 * 1024
#define MIN_BUFFER_READ   128

/*------------------------------------------------------------*/
/* private socket functions                                   */
/*------------------------------------------------------------*/
int crest_read_more(crest_connection *connection) {
  // MAX_BUFFER_READ effectively acts as a high water mark, and
  // MIN_BUFFER_READ as a low water mark - when there is less
  // than MIN_BUFFER_READ bytes free in the buffer, the buffer
  // is increased by MAX_BUFFER_READ bytes. The loop will
  // attempt to read the number of free bytes available,
  // bounding reads to be between MIN_BUFFER_READ and
  // MAX_BUFFER_READ bytes.
  
  int free_bytes = connection->request_buffer_length - connection->request_data_length;
  if(free_bytes < MIN_BUFFER_READ) {
    // TODO: handle realloc failure
    connection->request_buffer_length += MAX_BUFFER_READ;
    connection->request_buffer = (char *) realloc(connection->request_buffer, connection->request_buffer_length);
    free_bytes += MAX_BUFFER_READ;
  }
  
  // TODO: handle EOF
  int bytes_read = read(connection->client, connection->request_buffer + connection->request_data_length, free_bytes);
  if(bytes_read == -1)
    return CREST_READ_ERROR;
  connection->request_data_length += bytes_read;
  return CREST_READ_OK;
}

int crest_read_line(crest_connection *connection) {
  int line_length = 0, read_necessary = 1;
  char *buffer_end;
  
  // setup an initial buffer if one doesn't exist
  if(!connection->request_buffer) {
    // TODO: handle malloc error
    connection->request_buffer = (char *) malloc(MAX_LINE_LENGTH);
    connection->request_buffer_length = MAX_LINE_LENGTH;
    connection->line_start = connection->request_buffer;
    connection->line_end = connection->request_buffer;
  }
  
  // move line_start to the next line if at least one
  // line has been read previously
  if(*connection->line_end == '\n')
    connection->line_start = connection->line_end + 1;
  
  // an initial read isn't necessary if there are at least 2
  // bytes remaining in the buffer (to match CRLF)
  if(((connection->request_buffer + connection->request_data_length) - connection->line_start) >= 2)
    read_necessary = 0;
  
  while(line_length < MAX_LINE_LENGTH) {
    if(read_necessary)
      crest_read_more(connection);
    read_necessary = 1;
    
    // move line_end up until end of string or LF
    connection->line_end = connection->line_start;
    buffer_end = connection->request_buffer + connection->request_data_length;
    while((connection->line_end != buffer_end) && (*connection->line_end != LF))
      connection->line_end++;
    
    // ensure there are at least 2 characters in the line, and
    // try to match a CRLF pair
    if(*connection->line_end == LF && (connection->line_end != connection->line_start) && (*(connection->line_end - 1) == CR))
      return CREST_READ_OK;
    line_length = connection->line_end - connection->line_start;
  }
  
  return CREST_READ_ERROR;
}


/*------------------------------------------------------------*/
/* private protocol functions                                 */
/*------------------------------------------------------------*/
// parse request line: method SP URI SP HTTP/major.minor CRLF
int crest_parse_request_line(crest_connection *connection) {
	char *start, *end, *ptr = connection->line_start;
  assert(connection);
	
  switch(((uint32_t *)ptr)[0]) {
    case 'GET ':
      connection->method = http_get;
      ptr += 3;
      break;
    
    case 'POST':
      connection->method = http_get;
      ptr += 4;
      break;
    
    case 'PUT ':
      connection->method = http_post;
      ptr += 3;
      break;
    
    case 'PATC':
      connection->method = http_patch;
      ptr += 5;
      break;
    
    case 'DELE':
      connection->method = http_delete;
      ptr += 6;
      break;
    
    case 'HEAD':
      connection->method = http_head;
      ptr += 4;
      break;
    
    case 'OPTI':
      connection->method = http_options;
      ptr += 7;
      break;
    
    default:
      // TODO: respond with unknown method
      return CREST_PARSE_ERROR;
  }
  
  // ensure we matched a method name correctly, by checking if
  // there's a space where we expect it
  // TODO: respond with unknown method
  if(*ptr != ' ')
    return CREST_PARSE_ERROR;

  // tokenise the URI
  // TODO: honour MAX_URI_LENGTH
  start = ++ptr;
	move_to_end_of_uri(ptr);
	// TODO: respond with unknown method
	if(*ptr != ' ')
    return CREST_PARSE_ERROR;
  *ptr = 0;
  ptr++;
  connection->uri = strdup(start);

	// match 'HTTP/'
	if(ptr[0] == 'H' && ptr[4] == '/')
		start += HTTP_VERSION_PREFIX_LEN;
	else
		return CREST_PARSE_ERROR;
	
	// major version number
  start = ptr;
	move_to_end_of_digits(ptr);
	if(*ptr != '.')
    return CREST_PARSE_ERROR;
  *ptr = 0;
	connection->http_major_version = (unsigned long) strtol(start, NULL, 10);
	
	// minor version number
	start = ++ptr;
	move_to_end_of_digits(ptr);
	if(*ptr != '\r')
    return CREST_PARSE_ERROR;
  *ptr = 0;
	connection->http_minor_version = (unsigned long) strtol(start, NULL, 10);
	
	// check to make sure we're at the end of the line. CR was
	// matched above, there should be 1 more character available
	if(ptr != (connection->line_end - 1))
	  return CREST_PARSE_ERROR;
	else
		return CREST_PARSE_OK;
}

// parse header line: token(name) : white_space TEXT(value)
int crest_parse_header_line(crest_connection *connection) {
  // the final header is followed by a blank line and CRLF pair to
  // signify the start of the request body
  if((connection->line_end - connection->line_start) == 1) {
    connection->body_offset = (connection->line_end + 1) - connection->request_buffer;
    return CREST_PARSE_OK;
  }
  
  // tokenise header name and value
  char *ptr = connection->line_start;
  move_to_end_of_token(ptr);
  if(*ptr != ':')
    return CREST_PARSE_ERROR;
  *ptr = 0;
  move_to_end_of_ws(ptr);
  
  // make space for a new header
  int new_headers_count = ++connection->request_headers_count;
  connection->request_header_keys = realloc(connection->request_header_keys, new_headers_count * sizeof(char *));
  connection->request_header_values = realloc(connection->request_header_values, new_headers_count * sizeof(char *));
  
  // null terminate the header value after the last non WS char
  char *value = ptr;
  ptr = connection->line_end;
  
  while((ptr >= value) && ((*ptr == '\r') || (*ptr == '\n') || (*ptr == ' ') || (*ptr == '\t'))) {
    *ptr = 0;
    ptr--;
  }
  
  // copy the key and value. copies are made instead of storing
  // ptrs to strings within request_buffer, as request_buffer
  // may move during realloc
  connection->request_header_keys[new_headers_count - 1] = strdup(connection->line_start);
  connection->request_header_values[new_headers_count - 1] = strdup(ptr);
  
  return CREST_PARSE_OK;
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
    if(connection.request_headers_count > 0) {
      for(int i = 0; i < connection.request_headers_count; i++) {
        free(connection.request_header_keys[i]);
        free(connection.request_header_values[i]);
      }
      free(connection.request_header_keys);
      free(connection.request_header_values);
    }

    if(connection.response_headers_count > 0) {
      for(int i = 0; i < connection.response_headers_count; i++) {
        free(connection.response_header_keys[i]);
        free(connection.response_header_values[i]);
      }
      free(connection.response_header_keys);
      free(connection.response_header_values);
    }

    if(connection.uri)
      free(connection.uri);

    if(connection.request_buffer)
      free(connection.request_buffer);

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

