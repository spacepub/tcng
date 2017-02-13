/*
 * location.c - Source code location of language elements
 *
 * Written 2001,2002 by Werner Almesberger
 * Copyright 2001 EPFL-ICA
 * Copyright 2001,2002 Bivio Networks, Network Robots
 */

/*
 * KNOWN BUG: outputs tunnels once per selector @@@
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "data.h"
#include "tree.h"
#include "device.h"
#include "police.h"
#include "filter.h"
#include "qdisc.h"
#include "location.h"


char *file_name = NULL;
int lineno = 1;

static int file_name_ref = 0; /* reference count for "file_name" */


/* ----- Registry ---------------------------------------------------------- */


void input_file(const char *name)
{
    if (file_name && !file_name_ref) free(file_name);
    file_name = stralloc(name);
    file_name_ref = 0;
}


LOCATION current_location(void)
{
    LOCATION loc;

    loc.file = file_name && *file_name ? file_name : "<stdin>";
    loc.tag = NULL;
    loc.line = lineno;
    file_name_ref++;
    return loc;
}


int print_location(FILE *file,LOCATION loc)
{
    return fprintf(file,"%s:%d",loc.file,loc.line);
}


int print_current_location(FILE *file)
{
    return fprintf(file,"%s:%d",
      file_name && *file_name ? file_name : "<stdin>",lineno);
}


/* ----- File output helper functions -------------------------------------- */


static FILE *file_open(const char *name)
{
    FILE *file;

    fflush(stdout);
    fflush(stderr);
    if (!strcmp(name,"stderr")) return stderr;
    file = fopen(name,"w");
    if (file) return file;
    perror(name);
    exit(1);
}


static void file_close(FILE *file,const char *name)
{
    fflush(file);
    if (ferror(file)) errorf("error writing to \"%s\"",name);
    if (strcmp(name,"stderr"))
	if (fclose(file) == EOF) {
	    perror(name);
	    exit(1);
	}
}


/* ----- Dump location map ------------------------------------------------- */


static void do_map(FILE *file,LOCATION loc)
{
    const char *file_name,*walk;

    file_name = loc.file && *loc.file ? loc.file : "-";
    for (walk = file_name; *walk; walk++)
	if (*walk <= ' ' || *walk > '~') {
	    file_name = "<unprintable>";
	    break;
	}
    fprintf(file," %s %s %d\n",loc.tag ? loc.tag : "-",file_name,loc.line);
}


static void id_device(FILE *file,const DEVICE *dev)
{
    fprintf(file,"%s",dev->name);
}


static void id_qdisc(FILE *file,const QDISC *qdisc)
{
    id_device(file,qdisc->parent.device);
    if (qdisc->dsc == &ingress_dsc) fprintf(file,":ingress");
    else fprintf(file,":%u",(unsigned) qdisc->number);
}


static void id_class(FILE *file,const CLASS *class)
{
     id_qdisc(file,class->parent.qdisc);
     if (class->number == UNDEF_U32) fprintf(file,":unnumbered");
     else fprintf(file,":%u",(unsigned) class->number);
}


static void id_filter(FILE *file,const FILTER *filter)
{
    if (filter->parent.class) id_class(file,filter->parent.class);
    else {
	id_qdisc(file,filter->parent.qdisc);
	fprintf(file,":");
    }
    fprintf(file,":%u",(unsigned) filter->number);
}


static void id_tunnel(FILE *file,const TUNNEL *tunnel)
{
    id_filter(file,tunnel->parent.filter);
    fprintf(file,":%u",(unsigned) tunnel->number);
}


static void map_elements(FILE *file,const ELEMENT *elements)
{
    const ELEMENT *element;
    int n = 0;

    for (element = elements; element; element = element->next) {
	fprintf(file,"element ");
	id_filter(file,element->parent.filter);
	fprintf(file,":%d",n);
	do_map(file,element->location);
	if (element->parent.class == &class_is_tunnel) {
	    const ELEMENT *scan;

	    for (scan = elements; scan != element; scan = scan->next)
		if (scan->parent.class == &class_is_tunnel &&
		  scan->parent.tunnel == element->parent.tunnel)
		    break;
	    if (scan == element) {
		fprintf(file,"tunnel ");
		id_tunnel(file,element->parent.tunnel);
		do_map(file,element->parent.tunnel->location);
	    }
	}
	n++;
    }
}


static void map_filters(FILE *file,const FILTER *filters)
{
    const FILTER *filter;

    for (filter = filters; filter; filter = filter->next) {
	fprintf(file,"filter ");
	id_filter(file,filter);
	do_map(file,filter->location);
	map_elements(file,filter->elements);
    }
}


static void map_qdisc(FILE *file,const QDISC *qdisc);


static void map_classes(FILE *file,const CLASS *classes)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	fprintf(file,"class ");
	id_class(file,class);
	do_map(file,class->location);
	map_classes(file,class->child);
	map_qdisc(file,class->qdisc);
	map_filters(file,class->filters);
    }
}


static void map_qdisc(FILE *file,const QDISC *qdisc)
{
    if (!qdisc) return;
    fprintf(file,"qdisc ");
    id_qdisc(file,qdisc);
    do_map(file,qdisc->location);
    map_classes(file,qdisc->classes);
    map_filters(file,qdisc->filters);
}


static void map_devices(FILE *file)
{
    const DEVICE *device;

    for (device = devices; device; device = device->next) {
	fprintf(file,"device ");
	id_device(file,device);
	do_map(file,device->location);
	map_qdisc(file,device->ingress);
	map_qdisc(file,device->egress);
    }
}


static void map_policers(FILE *file)
{
    const POLICE *p;

    for (p = policers; p; p = p->next)
	if (p->used) {
	    fprintf(file,"police %u",(unsigned) p->number);
	    do_map(file,p->location);
	}
}


void write_location_map(const char *file_name)
{
    FILE *file;

    file = file_open(file_name);
    map_devices(file);
    map_policers(file);
    file_close(file,file_name);
}


/* ----- Look up location by specification --------------------------------- */


static const DEVICE *scan_device(const char **id)
{
    const DEVICE *dev;
    const char *end;
   
    end = strchr(*id,':');
    if (!end) end = strchr(*id,0);
    for (dev = devices; dev; dev = dev->next) {
	int len = strlen(dev->name);

	if (len != end-*id) continue;
	if (!strncmp(*id,dev->name,len)) {
	    *id = end;
	    return dev;
	}
    }
    return NULL;
}


static int scan_number(const char **id,uint32_t *res)
{
    int ok = 0;
    *res = 0;
    while (**id && **id != ':') {
	if (!isdigit(**id)) return 0;
	*res = *res*10+**id-'0';
	ok = 1;
	(*id)++;
    }
    return ok;
}


static const QDISC *find_qdisc(const QDISC *q,uint32_t num);


static const QDISC *find_qdisc_in_classes(const CLASS *classes,uint32_t num)
{
    const CLASS *c;

    for (c = classes; c; c = c->next) {
	const QDISC *q;

	if (c->qdisc) {
	    q = find_qdisc(c->qdisc,num);
	    if (q) return q;
	}
	q = find_qdisc_in_classes(c->child,num);
	if (q) return q;
    }
    return NULL;
}


static const QDISC *find_qdisc(const QDISC *q,uint32_t num)
{
    if (q->number == num) return q;
    return find_qdisc_in_classes(q->classes,num);
}


static const QDISC *scan_qdisc(const char **id)
{
    const DEVICE *dev;
    uint32_t num;

    dev = scan_device(id);
    if (**id != ':') return NULL;
    (*id)++;
    if (!strncmp(*id,"ingress",7) && (!(*id)[7] || (*id)[7] == ':')) {
	(*id) += 7;
	return dev->ingress;
    }
    if (!scan_number(id,&num)) return NULL;
    return find_qdisc(dev->egress,num);
}


enum qdisc_or_class {
    qoc_none,	/* not found or invalid */
    qoc_qdisc,	/* it's a qdisc */
    qoc_class	/* it's a class */
};


static const CLASS *find_class(const CLASS *classes,uint32_t num)
{
    const CLASS *class;

    for (class = classes; class; class = class->next) {
	const CLASS *c;

	if (class->number == num) return class;
	c = find_class(class->child,num);
	if (c) return c;
    }
    return NULL;
}


static enum qdisc_or_class scan_qdisc_or_class(const char **id,
  const QDISC **qdisc,const CLASS **class)
{
    const QDISC *q;
    const CLASS *c;
    uint32_t num;

    q = scan_qdisc(id);
    if (!q || **id != ':') return qoc_none;
    (*id)++;
    if (**id == ':') {
	if (qdisc) *qdisc = q;
	return qoc_qdisc;
    }
    if (!scan_number(id,&num)) return qoc_none;
    c = find_class(q->classes,num);
    if (c) {
	if (class) *class = c;
	return qoc_class;
    }
    return qoc_none;
}


static const FILTER *scan_filter(const char **id)
{
    const QDISC *q;
    const CLASS *c;
    const FILTER *f;
    uint32_t num;

    switch (scan_qdisc_or_class(id,&q,&c)) {
	case qoc_none:
	    return NULL;
	case qoc_qdisc:
	    f = q->filters;
	    break;
	case qoc_class:
	    f = c->filters;
	    break;
	default:
	    abort();
    }
    if (**id != ':') return NULL;
    (*id)++;
    if (!scan_number(id,&num)) return NULL;
    while (f && f->number != num)
	f = f->next;
    return f;
}


static const LOCATION *device_location(const char *id)
{
    const DEVICE *dev;

    dev = scan_device(&id);
    return *id || !dev ? NULL : &dev->location;
}


static const LOCATION *qdisc_location(const char *id)
{
    const QDISC *q;

    q = scan_qdisc(&id);
    return *id || !q ? NULL : &q->location;
}


static const LOCATION *class_location(const char *id)
{
    const CLASS *c;

    if (scan_qdisc_or_class(&id,NULL,&c) != qoc_class) return NULL;
    if (*id) return NULL;
    return &c->location;
}


static const LOCATION *filter_location(const char *id)
{
    const FILTER *f;

    f = scan_filter(&id);
    return *id || !f ? NULL : &f->location;
}


static const LOCATION *tunnel_location(const char *id)
{
    const FILTER *f;
    const ELEMENT *e;
    uint32_t num;

    f = scan_filter(&id);
    if (!f || *id != ':') return NULL;
    id++;
    if (!scan_number(&id,&num)) return NULL;
    if (*id) return NULL;
    for (e = f->elements; e; e = e->next)
	if (e->parent.class == &class_is_tunnel &&
	  e->parent.tunnel->number == num)
	    return &e->parent.tunnel->location;
    return NULL;
}


static const LOCATION *element_location(const char *id)
{
    const FILTER *f;
    const ELEMENT *e;
    uint32_t num;

    f = scan_filter(&id);
    if (!f || *id != ':') return NULL;
    id++;
    if (!scan_number(&id,&num)) return NULL;
    if (*id) return NULL;
    for (e = f->elements; e; e = e->next)
	if (e->number == num) return &e->location;
    return NULL;
}


static const LOCATION *police_location(const char *id)
{
    const POLICE *p;
    uint32_t num;

    if (!scan_number(&id,&num)) return NULL;
    if (*id) return NULL;
    for (p = policers; p; p = p->next)
	if (p->number == num) return &p->location;
    return NULL;
}


#define SEL(name) \
  if (!strncmp(spec,#name " ",strlen(#name)+1)) \
    return name##_location(spec+strlen(#name)+1);

const LOCATION *location_by_spec(const char *spec)
{
    SEL(device);
    SEL(qdisc);
    SEL(class);
    SEL(filter);
    SEL(tunnel);
    SEL(element);
    SEL(police);
    return NULL;
}

#undef SEL


/* ----- Dump variable use information ------------------------------------- */


typedef struct _var_register {
    const char *name;
    DATA data;
    struct _var_register *next;
} VAR_REGISTER;

static VAR_REGISTER *var_register = NULL,*last_var = NULL;


static void do_register_var(const char *name,DATA d)
{
    VAR_REGISTER *new;

    new = alloc_t(VAR_REGISTER);
    new->name = name;
    new->data = d;
    new->next = NULL;
    if (last_var) last_var->next = new;
    else var_register = new;
    last_var = new;
}


void register_var(const char *name,const DATA_ASSOC *entry,DATA d)
{
    const DATA_ASSOC *walk;
    int length = strlen(name);
    char *buf;

    for (walk = entry; walk; walk = walk->parent) {
	length += strlen(walk->name);
	if (!no_struct_separator) length++;
    }
    buf = alloc(length+1);
    buf[length] = 0;
    for (walk = entry; walk; walk = walk->parent) {
	length -= strlen(walk->name);
	memcpy(buf+length,walk->name,strlen(walk->name));
	if (!no_struct_separator) buf[--length] = '.';
    }
    memcpy(buf,name,strlen(name));
    do_register_var(buf,d);
}


/*
 * Special markers for scope boundary entries
 */

static char magic_begin_scope[1],magic_end_scope[1],magic_ignore[1];


void var_use_begin_scope(DATA d)
{
    do_register_var(magic_begin_scope,d);
}


void var_use_end_scope(void)
{
    assert(last_var);
    if (last_var->name == magic_begin_scope) last_var->name = magic_ignore;
    else do_register_var(magic_end_scope,data_none());
}


static void id_data(FILE *file,DATA d)
{
    if (d.op) {
	fprintf(file,"expression");
	return;
    }
    switch (d.type) {
	case dt_device:
	    fprintf(file,"device ");
	    id_device(file,d.u.device);
	    break;
	case dt_qdisc:
	    fprintf(file,"qdisc ");
	    id_qdisc(file,d.u.qdisc);
	    break;
	case dt_class:
	    fprintf(file,"class ");
	    id_class(file,d.u.class);
	    break;
	case dt_filter:
	    fprintf(file,"filter ");
	    id_filter(file,d.u.filter);
	    break;
	case dt_tunnel:
	    fprintf(file,"tunnel ");
	    id_tunnel(file,d.u.tunnel);
	    break;
	case dt_police:
	case dt_bucket: /* @@@ */
	    fprintf(file,"police %u",(unsigned) d.u.police->number);
	    break;
	default:
	    print_data(file,d);
    }
}


static void dump_var_assoc(FILE *file,const DATA_ASSOC *assoc)
{
    if (!assoc) return;
    dump_var_assoc(file,assoc->parent);
    fprintf(file,"%s%s",no_struct_separator ? "" : ".",assoc->name);
}


static void dump_var_value(FILE *file,const char *name,DATA d,
  const DATA_ASSOC *assoc)
{
    if (d.type != dt_assoc) {
	fprintf(file,"$%s",name);
	dump_var_assoc(file,assoc);
	fprintf(file," = ");
	id_data(file,d);
	fputc('\n',file);
    }
    else {
	const DATA_ASSOC *entry;

	for (entry = d.u.assoc; entry; entry = entry->next)
	    dump_var_value(file,name,entry->data,entry);
    }
}


static void dump_var_use(FILE *file,const VAR_REGISTER *var)
{
    if (var->name == magic_ignore) return;
    if (var->name == magic_end_scope) {
	fprintf(file,"}\n");
	return;
    }
    if (var->name == magic_begin_scope) {
	if (var->data.type == dt_none && !var->data.op) {
	    fprintf(file,"{\n");
	    return;
	}
	fprintf(file,"{ ");
	id_data(file,var->data);
	fputc('\n',file);
    }
    else {
	dump_var_value(file,var->name,var->data,NULL);
	data_destroy(var->data);
    }
}


void write_var_use(const char *file_name)
{
    FILE *file;

    file = file_open(file_name);
    while (var_register) {
	VAR_REGISTER *next;

	dump_var_use(file,var_register);
	next = var_register->next;
	free(var_register);
	var_register = next;
    }
    file_close(file,file_name);
}


/* ---- Support function for integrity checks ------------------------------ */


int var_any_class_ref(const struct _class *class)
{
    VAR_REGISTER *reg;

    for (reg = var_register; reg; reg = reg->next) {
	if (!*reg->name) continue; /* magic_* */
	if (reg->data.type == dt_class && reg->data.u.class == class) return 1;
    }
    return 0;
}
