#ifndef __TYPES_H
#define __TYPES_H

#define TYPE_STRING	1
#define TYPE_INTEGER 	2
#define TYPE_FLOAT 	3

typedef struct const_type {
  int type; /* TYPE_STRING or TYPE_INTEGER or TYPE_FLOAT */
  union {
    int     int_val;
    double  fp_val;
    char    *str_val;
  } u;
} const_type;

typedef struct node_type {
  int type;
} node_type;

#endif
