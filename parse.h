#ifndef PARSE_H

#define PARSE_H

# define MAX_LINE_SIZE 256

struct cg_server;

int
parse_rc_line(struct cg_server *server, char *line);

#endif /* end of include guard PARSE_H */
