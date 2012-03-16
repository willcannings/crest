#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*------------------------------------------------------------*/
/* routes finite state machine                                */
/*------------------------------------------------------------*/
// TODO: dynamic state->transition arrays
#define MAX_TRANSITIONS 100
#define MAX_STATES      100

typedef enum {
  character,
  parameter,
  end_of_path
} transition_type;

typedef struct {
  transition_type type;
  char transition_character;
  char *parameter_name;
  void *state;
} transition;

typedef enum {
  intermediate,
  start_point,
  end_point
} state_type;

typedef struct {
  state_type type;
  transition *transitions[MAX_TRANSITIONS];
  int transitions_count;
  int non_null_transitions_count;
  char *function_name;
  int index;
} state;


/*------------------------------------------------------------*/
/* routes file parser                                         */
/*------------------------------------------------------------*/
static state *end_states[MAX_STATES];
static state *start_states[MAX_STATES];
static int routes_count = 0;

// TODO: implement parameter globbing (:id)
// TODO: add http verbs as an option for routes, e.g
// show_book GET /books/:id
// delete_book DELETE /books/:id

void parse_line(char *line, char *end_line, int line_number) {
  // start and end states of the transition
  state *start_state, *end_state, *previous_state, *current_state;
  start_state = (state *) calloc(1, sizeof(state));
  end_state = (state *) calloc(1, sizeof(state));
  start_state->type = start_point;
  end_state->type = end_point;
  
  // grab the function name for this route
  end_state->function_name = line;
  while((line < end_line) && (!isspace(*line)))
    line++;
  
  // if the end of the function name is followed by a whitespace
  // char, skip the whitespace. without a whitespace char, no url
  // can follow the function name
  if(line < end_line) {
    *line = 0;
    line++;
    
    while((line < end_line) && isspace(*line))
      line++;
    
    if(*line != '/') {
      printf("Error: The URL on line #%d must start with '/'\n", line_number);
      exit(1);
    }
  } else {
    printf("Error: Expected a URL after the function name on line #%d\n", line_number);
    exit(1);
  }
  
  // as states are inserted between the start and end states, the
  // first transition of the last state is set to the end_transition
  // (pointing to end). When a new state is added, the transition to
  // end_transition is replaced with a new transition to the next
  // state, and its first transition is set to end_transition.
  transition *end_transition, *current_transition;
  end_transition = (transition *) calloc(1, sizeof(transition));
  end_transition->type = end_of_path;
  end_transition->state = end_state;
  start_state->transitions[start_state->transitions_count++] = end_transition;
  previous_state = start_state;
  
  while((line < end_line) && !isspace(*line)) {
    current_state = (state *) calloc(1, sizeof(state));
    current_state->type = intermediate;
    current_state->transitions[current_state->transitions_count++] = end_transition;
    
    current_transition = (transition *) calloc(1, sizeof(transition));
    current_transition->type = character;
    current_transition->transition_character = *line;
    current_transition->state = current_state;    
    
    previous_state->transitions[0] = current_transition;
    previous_state = current_state;
    line++;
  }
  
  start_states[routes_count] = start_state;
  end_states[routes_count] = end_state;
  routes_count++;
}


/*------------------------------------------------------------*/
/* state machine processor                                    */
/*------------------------------------------------------------*/
static int next_state_index = 1;

// a transition is equivalent to another if they are of the
// same type, have the same parameter, and end up at a state
// that is of the same type.
int transition_equivalent(transition *a, transition *b) {
  return  (a->type == b->type) &&
          (a->transition_character == b->transition_character) &&
          (((state *)a->state)->type == ((state *)b->state)->type);
}

// merging the state machines created by each url involves
// collapsing transitions to states that are equivalent.
// After collapsing, each new path is rescursively followed
// to merge further paths.
void collapse(state *start) {
  start->non_null_transitions_count = start->transitions_count;
  for(int i = 0; i < start->transitions_count; i++) {
    if(start->transitions[i] == NULL)
      continue;
    state *state_a = (state *) start->transitions[i]->state;
    state_a->index = next_state_index++;
      
    for(int j = i + 1; j < start->transitions_count; j++) {
      if(start->transitions[j] == NULL)
        continue;
      
      // remove equivalent transitions, and copy the outbound
      // transitions from state_b onto state_a, merging the
      // equivalent transitions and states together.
      if(transition_equivalent(start->transitions[i], start->transitions[j])) {
        state *state_b = (state *) start->transitions[j]->state;
        start->transitions[j] = NULL;
        start->non_null_transitions_count--;
        for(int k = 0; k < state_b->transitions_count; k++)
          state_a->transitions[state_a->transitions_count++] = state_b->transitions[k];
      }
    }
    
    collapse(state_a);
  }
}


/*------------------------------------------------------------*/
/* output generator                                           */
/*------------------------------------------------------------*/
void print_transition_char(transition *trans) {
  if(trans->type == character) {
    printf("'%c'", trans->transition_character);
  } else if(trans->type == end_of_path) {
    printf("'\\0'");
  }
}

void switch_for_state(state *current_state) {
  if(current_state->index != 0)
    printf("\tstate%d:\n", current_state->index);
  
  // at an end point, call the function corresponding to a matched url
  if(current_state->type == end_point) {
    printf("\t%s(connection);\n\treturn 1;\n\n", current_state->function_name);
    
  } else if(current_state->type == intermediate || current_state->type == start_point) {
    // for a single transition from a state, use an if statement
    if(current_state->non_null_transitions_count == 1) {
      for(int i = 0; i < current_state->transitions_count; i++) {
        if(current_state->transitions[i] != NULL) {
          state *transition_state = (state *)current_state->transitions[i]->state;
          printf("\tif(*url == ");
          print_transition_char(current_state->transitions[i]);
          printf(")\n\t\tgoto state%d;\n\telse\n\t\treturn 0;\n\n", transition_state->index);
          break;
        }
      }
      
    // for multiple transitions from a state, use a switch statement
    } else if(current_state->non_null_transitions_count > 1) {
      printf("\tswitch(*url) {\n");
      for(int i = 0; i < current_state->transitions_count; i++) {
        if(current_state->transitions[i] == NULL)
          continue;

        state *transition_state = (state *)current_state->transitions[i]->state;
        printf("\t\tcase ");
        print_transition_char(current_state->transitions[i]);
        printf(":\n\t\t\tgoto state%d;\n\t\t\tbreak;\n", transition_state->index);
      }
      printf("\t\tdefault:\n\t\t\treturn 0;\n");
      printf("\t}\n\n");
    }

    for(int i = 0; i < current_state->transitions_count; i++) {
      if(current_state->transitions[i] == NULL)
        continue;
      switch_for_state((state *)current_state->transitions[i]->state);
    }
  }
}

void generate_code(state *start) {
  printf("#include \"crest.h\"\n\n");
  for(int i = 0; i < routes_count; i++)
    printf("extern void %s(crest_connection *connection);\n", end_states[i]->function_name);
  printf("\nint match_url(char *url, crest_connection *connection) {\n");
  switch_for_state(start);
  printf("}\n");
}


/*------------------------------------------------------------*/
/* main                                                       */
/*------------------------------------------------------------*/
int main(int argc, char **argv) {
  if(argc < 2) {
    printf("Usage:\t%s input_path\n", argv[0]);
    printf("\tinput_path: crest route file path\n");
    exit(0);
  }
  
  // open the input file and determine file length to
  // create a buffer before reading
  // TODO: handle file open error
  FILE *file = fopen(argv[1], "rb");
  fseek(file, 0, SEEK_END);
  int file_length = ftell(file);
  rewind(file);
  
  if(file_length == 0) {
    printf("Error: the input file is empty\n");
    exit(1);
  }
  
  // read the entire file into the data buffer
  char *data = (char *) malloc(file_length + 1); // TODO: catch when data exceeds memory
  data[file_length] = 0;
  fread(data, 1, file_length, file);
  fclose(file);
  
  // the parse_line function reads the next line from
  // data and returns a new start state representing
  // the url and function defined on that line
  char *start = data, *end = data;
  int line_number = 1;
  
  while(*data) {
    // determine start and end points of the next line
    start = data;
    while(*data && (*data != '\n'))
      data++;
    end = data;
    
    // don't increment data if we've reached EOF
    if(*data == '\n')
      data++;
    
    // skip leading whitespace on the line
    while((start < end) && isspace(*start))
      start++;
    if(start == end)
      continue;
    
    // process the next line, adding its end and start states
    // to the global start_states and end_states arrays
    parse_line(start, end, line_number++);
  }
  
  if(routes_count == 0) {
    printf("Error: the input file contains no routes\n");
    exit(1);
  }
  
  // to merge the state machines created by each url,
  // collapse starts from a single state and recursively
  // moves through each path, collapsing equivalent
  // transitions together. To start this, each url's
  // start state is merged, so all state machine paths
  // can be followed from a single starting point.
  state *current_state, *start_state = start_states[0];
  for(int i = 1; i < routes_count; i++) {
    current_state = start_states[i];
    start_state->transitions[start_state->transitions_count++] = current_state->transitions[0];
  }
  
  collapse(start_state);
  generate_code(start_state);
  
  return 0;
}
