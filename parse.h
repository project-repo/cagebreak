#ifndef PARSE_H

#define PARSE_H

struct cg_server;

int
parse_rc_line(struct cg_server *server, char *line);

#endif /* end of include guard PARSE_H */
