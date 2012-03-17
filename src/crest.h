#ifndef __included_crest__
#define __included_crest__

typedef enum {
  http_get,
  http_post,
  http_put,
  http_patch,
  http_delete,
  http_head,
  http_options
} http_method;

typedef struct {
  // request
  char  *request_buffer;
  char  *line_start;
  char  *line_end;
  int   request_data_length;
  int   request_buffer_length;
  int   server;
  int   client;
	http_method method;
	char  *uri;
	int   http_major_version;
	int   http_minor_version;
  int   body_offset;
	char  *body;
	char  *remote_address;
  char  **request_header_keys;
  char  **request_header_values;
  int   request_headers_count;
  
  // response
  char  **response_header_keys;
  char  **response_header_values;
  int   response_headers_count;
  char  *response_body;
  int   response_length;
} crest_connection;


/*------------------------------------------------------------*/
/* external functions                                         */
/*------------------------------------------------------------*/
extern int match_url(char *url, crest_connection *connection);


/*------------------------------------------------------------*/
/* server functions                                           */
/*------------------------------------------------------------*/
void crest_start_server(int port);
void crest_write_string(crest_connection *connection, char *data);
void crest_write(crest_connection *connection, void *data, int length);
void crest_complete(crest_connection *connection);


/*------------------------------------------------------------*/
/* editable configuration values                              */
/*------------------------------------------------------------*/
#define MAX_URI_LENGTH				(10 * 1024)
#define MAX_HEADER_KEY_LENGTH 255
#define MAX_HEADER_VAL_LENGTH	(10 * 1024)


/*------------------------------------------------------------*/
/* response codes                                             */
/*------------------------------------------------------------*/
#define CREST_PARSE_ERROR			0
#define CREST_PARSE_OK				1
#define CREST_READ_ERROR      0
#define CREST_READ_OK         1


/*------------------------------------------------------------*/
/* tokens                                                     */
/*------------------------------------------------------------*/
#define SP							' '
#define HT							'\t'
#define CR							'\r'
#define LF							'\n'
#define CRLF						"\r\n"
#define CRLF_LEN					2
#define HTTP_VERSION_PREFIX_LEN		5   // "HTTP/"


/*------------------------------------------------------------*/
/* parsing helper macros                                      */
/*------------------------------------------------------------*/
// CTL = <any US-ASCII control character (octets 0 - 31) and DEL (127)>
#define not_ctl(s)			(*s > 31 && *s != 127)

// separators = "(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\" | <">
//            | "/" | "[" | "]" | "?" | "=" | "{" | "}" | SP | HT
#define not_separator(s)	(*s != 34 && *s != 40 && *s != 41 && *s != 44\
							&& *s != 47 && (*s < 58 || *s > 64) && *s != 91\
							&& *s != 92 && *s != 93 && *s != 123 && *s != 125\
							&& *s != 32 && *s != 9)


/*------------------------------------------------------------*/
/* parsing macros based directly on RFC 2616 (HTTP 1.1)       */
/*------------------------------------------------------------*/
// DIGIT = <any US-ASCII digit "0".."9">
#define move_to_end_of_digits(s)  while(*s && (*s >= '0') && (*s <= '9')) s++;

// token = 1*<any CHAR except CTLs or separators>
#define move_to_end_of_token(s)	  while(*s && not_ctl(s) && not_separator(s)) s++;

// TODO - check RFC for this definition
#define move_to_end_of_uri(s)		  while(*s && (*s != ' ') && not_ctl(s)) s++;

// TEXT = <any OCTET except CTLs, but including LWS>
// TODO: handle LWS here as per RFC
#define move_to_end_of_TEXT(s)	  while(*s && not_ctl(s)) s++;

// WS = 1*( SP | HT )
#define move_to_end_of_ws(s)      while(*s && (*s == ' ') || (*s == '\t')) s++;

#endif
