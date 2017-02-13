%
% tcng.inc - The tcng Language
%
% Written 2001-2004 by Werner Almesberger
% Copyright 2001 EPFL-ICA
% Copyright 2001,2002 Bivio Networks, Network Robots
% Copyright 2002-2004 Werner Almesberger
%


The description of the \prog{tcng} language is divided into four
principal chapters:
\begin{itemize}
  \item the current chapter introduces the most common language constructs
  \item chapter \ref{chphood} describes additional details which are needed
    when adding support for new protocols, meters, or even new targets
  \item chapter \ref{chphist} discusses historical constructs which can be
    found in older \prog{tcng} configurations, e.g. many of the regression
    tests included in the \prog{tcng} package
\end{itemize}

Besides imposing restrictions, a specific architecture may also add new
language elements, e.g. keywords for new traffic control elements.


%------------------------------------------------------------------------------


\section{Quick start}

The following example illustrates most of the elements found in a typical
\prog{tcng} configuration:

\begin{verbatim}
#include "fields.tc"
#include "ports.tc"

dev "eth0" {
    egress {
        // classification

        class (<$high>)
            if tcp_dport == PORT_HTTP;
        class (<$low>)
            if 1;

        // queuing

        prio {
            $high = class (1) {
                fifo (limit 20kB);
            }
            $low = class (2) {
                fifo (limit 100kB);
            }
        }
    }
}
\end{verbatim}

The first two lines include files with definitions for header fields
and port numbers. For details, see section \ref{tcnginc}.

The \verb"dev" and \verb"egress" lines determine what is being configured,
i.e. the egress (outbound) side of the network interface ``eth0''.

The configuration consists of two parts: the classification and the setup
of the queuing system. In this example, we use a priority scheduler with
two classes for the priorities ``high'' and ``low''.

Packets with TCP destination port 80 (HTTP) are sent to the high
priority class, while all other packets (\verb"if 1;") are sent to the
low priority class. Note that it is generally a good idea to finish a
classifier with a selection rule that is always true.

The queuing part defines the queuing discipline for static priorities,
with the two classes. Inside the high-priority class, there is another
queuing discipline -- a simple FIFO with a capacity of 20 kilobytes.
Likewise, the low-priority class contains a FIFO with 100 kB.


%------------------------------------------------------------------------------


\section{Basic syntax}
\label{tcngbasic}

The configuration is passed through \prog{cpp} for file inclusion and macro
substitution. C or C++ comments are removed by \prog{cpp}.

Examples:

\begin{verbatim}
    // this is a comment
    /* and, of
       course, this
       too */
\end{verbatim}

Language tokens can be separated by whitespace, which may include
newlines, i.e. the usual formatting rules of C or Perl apply.

In all cases where a string is expected, one can use a string in
double quotes or a string variable.

Examples:

\begin{verbatim}
host "localhost"         /* quoted string */
host "ftp.kernel.org"    /* another quoted string */
host $var                /* variable */
\end{verbatim}


%------------------------------------------------------------------------------


\section{Interfaces}

Each configuration file contains the traffic control configuration of one
or more network interfaces. Each interface section begins with the keyword
\name{dev}, followed by the interface name, and the elements configured at
that interface, in curly braces.
The interface name can be an arbitrary string expression.

Examples:

\begin{verbatim}
dev "eth0" {
    ...
}
\end{verbatim}

\begin{verbatim}
$number = "0";
dev "eth"+$number {
    ...
}
\end{verbatim}

In some cases, e.g. when using \prog{tcsim}, the interface name is omitted,
see section \ref{inthist}.


%------------------------------------------------------------------------------


\section{Queuing disciplines}

An interface section contains queuing discipline specifications, which
consist of the name of the queuing discipline, an optional list of
parameters, in parentheses and separated by commas, and an optional list
of items attached to this queuing discipline, in curly braces.

In each interface section, one egress and one ingress queuing discipline
can be defined.

Examples:

\begin{verbatim}
prio {
    class (1) ...;
    class (2) ...;
}

egress {
    ...
}

ingress {
    ...
}
\end{verbatim}

The list of items can contain filters (if the queuing discipline supports
them), classes (if the queuing discipline has classes), and further
queuing disciplines.

The latter is mainly used for queuing disciplines like
\name{ingress}, \name{egress}/\name{dsmark}%
\footnote{\name{dsmark} can be called \name{egress} when used as the
top-level egress queuing discipline of a device, which is the only place
where \name{dsmark} is normally used. Therefore, we use \name{dsmark}
and \name{egress} as synonyms in this document.}, or \name{tbf},
which have a
``central'' inner queuing discipline, but no queuing disciplines
attached to classes. Example:

\begin{verbatim}
egress {
    ...
    fifo;
}
\end{verbatim}


%------------------------------------------------------------------------------


\section{Classes}

A class specification begins with the keyword \name{class}, followed by an
optional list of parameters, in parentheses, zero or more selectors
(selectors are typically \name{if} constructs, see section \ref{ifsel}),
and an optional list of items attached to the class, in curly braces.

Examples:

\begin{verbatim}
// clear TOS byte of all packets with EF and no ECN bits set

dsmark(set_tc_index) {
    class (0x2e << 2, mask 0);
}
\end{verbatim}

\begin{verbatim}
egress {
    class (<$one>)
        if ip_dst == 10.0.0.1;
    class (<$two>)
        if ip_dst == 10.0.0.2;
    class (<$three>)
        if 1;

    prio {
        $one = class (1);
        $two = class (2);
        $three = class (3);
    }
}
\end{verbatim}

The number of a class is usually automatically assigned. It can be
explicitly set by including it among the parameters, e.g.

\begin{verbatim}
class (1) ...;
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{The \name{drop} construct}

A classifier can also be used to drop packets, just like a firewall does.
Using \name{if} (see below), this could be implemented as follows:

\begin{verbatim}
egress {
    class (<$whatever>))
        if ip_src:8 == 10.0.0.0 && drop;
    ...
}
\end{verbatim}

This is not very intuitive, and unnecessarily requires the presence (or
even creation) of a class. The \name{drop} construct allows this to be
expressed as:

\begin{verbatim}
egress {
    drop if ip_src:8 == 10.0.0.0;
    ...
}
\end{verbatim}


%------------------------------------------------------------------------------


\section{The \name{if} construct}
\label{ifsel}

Classes are selected using the \name{if} construct.

The \name{if} construct consists of the the keyword \name{if}, followed by a
Boolean expression, using the normal expression syntax. \prog{tcng} converts
this to an equivalent configuration for the classification mechanism(s)
provided by the respective ``target''.

Note that \name{if} clauses are considered part of the \name{class} construct
and are therefore not individually terminated with semicolons. Instead, a
single semicolon (or block) is put at the end of the \name{class}
construct.

Example:

\begin{verbatim}
class
    if something
    if something_else;
\end{verbatim}

or

\begin{verbatim}
class (...)
    if something
    if something_else
{
    ...
}
\end{verbatim}

but not

\begin{verbatim}
class (...)
    if something;     /* WRONG ! */
    if something_else;
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Data access}
\label{dataacc}

Packet data can be accessed with the expression
\raw{raw$[$\meta{offset}$]$.\meta{size}}
where \meta{offset} is the integer offset of the field, in bytes, and
\meta{size}
is an optional size qualifier. The following qualifiers are supported:

\begin{tabular}{ll}
  \name{.b}  & Byte (default) \\
  \name{.ns} & ``Network short'', two bytes in network byte order \\
  \name{.nl} & ``Network long'', four bytes in network byte order \\
  \name{.ipv4} & IPv4 address, four bytes in network byte order \\
  \name{.ipv6} & IPv6 address, sixteen bytes in network byte order \\
\end{tabular}

If the offset is zero, the square brackets and the offset expression
can be omitted.

\name{raw} only provides the basic access mechanism. A more elegant way
for using header fields is described in the next section.

Examples:

\begin{verbatim}
/* select IPv4 packets without IP options */
class ...
   if raw[0] == 0x45;
\end{verbatim}

\begin{verbatim}
/* select IPv4 packets with the "don't fragment" bit set */
class ...
   if (raw & 0xf0) == 0x40 && (raw[6].b & 4) == 4;
\end{verbatim}

The choice of expressions is limited by what \prog{tcng} knows to translate,
and by what the respective targets support. There are the following general
restrictions:

\begin{itemize}
  \item a basic Boolean expression consists of the comparison of a constant
    value with the content of a field
  \item field values can be combined with constant bit shifts and binary
    ``and'' (\verb"&")
  \item in some simple cases, also other arithmetic or bit operations can
    be applied to field values, e.g. adding or subtracting a constant,
    multiplying with a power of two, etc.
  \item an integer constant or a constant expression yielding an integer
    result is also a basic Boolean expression
  \item a field access without relational operator is treated as if it
    was of the form \raw{\meta{access} != 0}
  \item basic Boolean expressions can be combined with
    \verb"&&", \verb"||", and \verb"!"
\end{itemize}

Examples of basic Boolean expressions:

\begin{verbatim}
class ...
    if ip_proto == 123
    if (ip_tos & 0xfc) == 0xb8
    if (tcp_sport-4) < 1020;
\end{verbatim}

Combined basic expressions:

\begin{verbatim}
class ...
    if ip_src == 10.0.0.0 || ip_dst == 10.0.0.0
    if ip_dst:8 == 9.0.0.0 &&
      (udp_dport == 53 || tcp_dport == 53);
\end{verbatim}

These are not valid expressions:

\begin{verbatim}
class ...
   /* WRONG: result of comparison is not a basic
      Boolean expression */
   if (tcp_dport == 80) == 1
   /* WRONG: cannot compare fields with each other */
   if ip_src == ip_dst;
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Fields}
\label{fields}

Instead of using the slightly awkward \name{raw} expressions, header
fields can be defined such that they can be accessed by name, without
the need to calculate offsets, etc., each time.

Fields in a packet are declared using the \name{field} construct,
which consists of the keyword \name{field}, the field name (which must not
be the same as any other keyword of the tcng language), an equal sign,
an expression, and optionally a precondition consisting of the keyword
\name{if} followed by a Boolean expression.

If a precondition is specified, the field and all fields derived from
it only exist if the precondition is true. Note that
\raw{\meta{field} == \meta{value}}
and \raw{\meta{field} != \meta{value}}
are always false if the preconditions of the field are not met !

If a field expression consists only of a field, an index, and possibly
a size qualifier, this expression can in turn be used as the basis for
an index operation.

Examples:

\begin{verbatim}
field ip = raw;
field ip_hl = ip[0] & 0xf;
field ip_nexthdr = ip[ip_hl << 2];
field tcp = ip_nexthdr if ip_proto == IPPROTO_TCP;
field tcp_SYN = (tcp_hdr[13].b >> 1) & 1;
\end{verbatim}

Several fields commonly used with IPv4 are defined in
\url{./tcng/lib/tcng/include/fields.tc} (when using a local copy of
\prog{tcng}) or \url{/usr/lib/tcng/include/fields.tc} (when \prog{tcng}
is installed system-wide), see section \ref{fieldstc}.



%------------------------------------------------------------------------------


\section{Expressions}

Almost at any place where a value is expected, one can use an expression.

The expression syntax of \prog{tcng} is similar to programming languages
like C or Perl.

Boolean expressions have already been covered in section \ref{dataacc}.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Numbers}

Numbers can be unsigned 32 bit integers, IPv4 addresses, IPv6 addresses,
or floating-point numbers.
Arithmetic operators use integer arithmetic unless one of their arguments
is a
floating-point numbers.

Like Perl, \prog{tcng} supports not only decimal, hexadecimal, and octal,
but also binary integer constants:

\begin{tabular}{ll}
  \tt 123  & decimal, 123 \\
  \tt 0x40 & hexadecimal, decimal value 64 \\
  \tt 0100 & octal, decimal value 64 \\
  \tt 0b101 & binary, decimal value 5 \\
\end{tabular}

IPv4 addresses in dotted quad form (e.g. \verb"216.136.171.196") are
also treated as unsigned integers, in host byte order. Similarly,
IPv6 addresses in any of the formats described in \cite{RFC2373} are
treated as unsigned 128 bit integers.

IPv4 and IPv6 addresses can be used like other integers, except
\begin{itemize}
  \item on the right-hand side of a shift operator
  \item as the second or third operand of the mask operator (see section
    \ref{ops})
  \item in multiplication, division, or modulo
\end{itemize}
Also, the \verb"/" operator works as a mask operator if the left
operand is an address, see section \ref{ops}.

Examples:

\begin{tabular}{lll}
  \verb"10/3" & equals & 3 \\
  \verb"5/2.0" & equals & 2.5 \\
  \verb"10.1.2.3 & 0xff000000" & equals & 10.0.0.0 \\
  \verb"10.1.2.3/8" & equals & 10.0.0.0 \\
\end{tabular}

\prog{tcng} also supports negative numbers, but they are simply
treated as 32 bit unsigned integers that have wrapped around zero,
so multiplications and divisions involving negative numbers may return
unexpected results.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Units}
\label{units}

Numbers representing physical quantities must have their unit explicitly
specified. This is done by using a unit qualifier. The following units are
recognized:

\begin{tabular}{ll}
  \name{bps} & bits per second \\
  \name{Bps} & bytes per second \\
  \name{b}   & bits (aliases: \name{bit}, \name{bits}) \\
  \name{B}   & bytes (aliases: \name{Byte}, \name{Bytes}) \\
  \name{p}   & packets (aliases: \name{pck}, \name{pcks}) \\
  \name{pps} & packets per second \\
  \name{s}   & seconds (aliases: \name{sec}, \name{secs}) \\
\end{tabular}

Unit qualifiers can be prefixed with \name{m} (milli), \name{k} (kilo),
\name{M} (mega), or \name{G} (giga). Note that \prog{tcng} prints a warning
if using the \name{m} prefix for anything but seconds. The prefixes are
usually powers of 1000, except when applied to bytes and bits, where they
become powers of 1024.

Arithmetic operators check and adjust units. Numbers with units cannot be
used with logical or bit operators.

Examples:

\begin{tabular}{ll}
  \raw{1Mbps} & are 1000000 bits per second \\
  \raw{2kB}   & are 2048 bytes \\
  \raw{10.0e6b/1sec} & are 10 Mbps \\
\end{tabular}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Strings}

String constants consist of a sequence of zero or more printable characters,
including spaces, surrounded by double quotes. Control characters are not
allowed in strings, and the use of non-ASCII characters (e.g. accented
characters) is discouraged. Unlike in C, the backslash has no special
meaning in a string.

Strings can be concatenated with \verb"+", and compared with the relational
operators. Note that, unlike \prog{Perl}, \prog{tcng} does not try to convert
strings to numbers in this case.

Examples:

\begin{tabular}{ll}
  \verb|"local"+"host"| & yields \verb|"localhost"| \\
  \verb|"010" == "10"|  & is false \\
\end{tabular}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Operators}
\label{ops}

All operators known from C are available, with the following exceptions:

\begin{itemize}
  \item assignment is a statement, not an operator 
  \item no comma operator
  \item no \verb"? :" operator
  \item no \name{cast} or \name{sizeof} operator
  \item no pointer-related operators (\verb"&", \verb"->", etc.)
  \item \verb"/" acts a mask operator (see below) when the left operand is an
    IPv4 or IPv6 address, e.g. \verb"1.2.3.4/8", \verb"1.2.3.4:8", and
    \verb"1.0.0.0" are all equivalent.
\end{itemize}

The following new operators have been added:

\begin{itemize}
  \item the mask operator
    \raw{\meta{value}:$[$\meta{left}$][$:$[$\meta{right}$]]$}
    \begin{description}
      \item[\raw{\meta{value}:\meta{left}}] zeroes all bits but the upper
        \meta{left}
      \item[\raw{\meta{value}: :\meta{right}}]
        zeroes all bits but the lower \meta{right}.

        Note that there is a syntactical ambiguity between the zero IPv6
	address (\verb"::") and this form of the mask operator. It is
	therefore recommended to always put a space between the two colons
	if using the mask operator.
      \item[\raw{\meta{value}:\meta{left}:\meta{right}}]
        zeroes all bits but the next \meta{right} after skipping the upper
        \meta{left}
    \end{description}
    When using an integer or an IPv4 address, the value is always considered
    to have 32 bits when counting ``upper'' bits. When using an IPv6 address,
    the value is considered to have 128 bits.
  \item unit qualifiers (see above)
  \item relational operators and plus also work on strings
  \item the \name{host} operator \raw{host \meta{integer}} or
     \raw{host \meta{string}}, which returns an IPv4 address. When used on
     a string or string expression, \name{host} performs a host name lookup
     to obtain the address.
  \item the \name{host4} operator, which is equivalent to the \name{host}
    operator described above
  \item the \name{host6} operator, which works like \name{host}, but for
    IPv6 addresses instead of IPv4 addresses
  \item the \name{precond} operator, see section \ref{precond}
\end{itemize}

Operator precedence is exactly as in C. In order to leave the precedence
of C operators undisturbed, the mask operator and unit qualifiers have been
given the highest precedence, while \name{host}, \name{host4}, \name{host6},
and \name{precond} can be used like functions.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Functions}

\prog{tcng} currently only has one function: the formatted output
function \name{sprintf} that returns a string, like the function
with the same name in Perl.

The usage is like in C or Perl, and the following components are recognized
in a conversion specification (see section B1.2 of \cite{KR88} for further
details):
\begin{itemize}
  \item a leading \verb"-" for left adjustment
  \item \verb"0" for leading zeroes
  \item the minimum field width, or \verb"*"
  \item if the conversion character is \verb"f" or \verb"s": the
    precision, or \verb"*"
  \item the conversion characters \verb"d", \verb"u", \verb"o", \verb"x",
    \verb"X", \verb"f", \verb"c", \verb"s", and \verb"%"
\end{itemize}

When using the conversion character \verb"%", the whole specification
must be \verb"%%".

Example:
\begin{verbatim}
$tag = "item"+sprintf("%04d",$number);
\end{verbatim}


%------------------------------------------------------------------------------


\section{Variables}

Values and expressions can be stored in variables. Variable assignments
can be placed anywhere where a queuing discipline, class, or filter could
be specified.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Variable basics}

Like in Perl, variable names begin with a dollar sign, followed by a
letter or an underscore, optionally followed by more letters, digits, or
underscores.

Also like in Perl, variables can contain strings.

Examples:

\begin{verbatim}
$five = 5;
$answer = ($five << 3)+2;
$rate = 135.5 Mbps;
$host = "www.foo.edu";
\end{verbatim}

If an expression stored in a variable is not constant, it is inserted
``as is'' wherever the variable is referenced. Note that any references
to fields or other variables are resolved at the time of assignment.

Example:

\begin{verbatim}
$cond = ip_src == 1.2.3.4 && tcp_sport == 80;
...
    class (1) if $cond;
...
field tcp_sport = ...;  // does not change $cond
...
    class (2) if $cond;
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Variable scoping}

Scoping is similar to C: variables defined within curly braces are not
visible beyond the closing brace. It is an error to try to access an
undefined variable. Note that it is not possible to change the value of
a variable defined at an outer scope -- instead, a new variable at the
inner scope is created.

Like in C or Perl, blocks can be created inside other blocks, which is
mainly useful when writing macros.

Example:

\begin{verbatim}
prio {
    {
        $x = 1;
        class ($x);
    }
    {
        $x = 2;
        class ($x);
    }
}
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Forward references}

Using a variable before a value is assigned to it is
called a forward reference. \prog{tcng} uses forward references in the
following cases:

\begin{itemize}
  \item the list of classes in \name{priomap}\footnote{For a description of
  \name{priomap}, please refer to the section on \name{prio} in \cite{LARTC}.}
  \item the set of classes in a class selection path
\end{itemize}

More details on these cases can be found in sections \ref{parameters}
and \ref{tcngcsp}, respectively.

Forward-referenced variables have the scope of the location where they
are first referenced. They cannot be read before a value has been
assigned to them, but they can be used for further forward references.


%------------------------------------------------------------------------------


\section{Parameters}
\label{parameters}

Most elements, such as queuing disciplines, their classes, meters, etc.,
can have parameters.

A complete list of parameters can be found in appendix \ref{appparam}.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Parameter syntax}

A parameter consists of the parameter name, followed by the parameter
value, which can typically be an expression. Flag parameters have no
value. Parameters are separated
by commas. A parameter may only appear once in the same parameter list.

Examples:

\begin{verbatim}
police (rate 100kbps, burst 2*1kB, mtu $mtu);
\end{verbatim}

\begin{verbatim}
class (rate 1Mbps,prio 1,isolated); /* CBQ */
\end{verbatim}

In the following cases, the parameter value takes a special form:
\begin{description}
  \item[\name{priomap}:] the value is a space-separated list of
    forward-referenced variables, e.g. \verb"priomap $a $b $c"
  \item[\name{pragma}:] the value is a space-separated list of strings,
    see section \ref{pragmas}
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Parameter propagation}

Most parameters are automatically propagated from a queuing discipline
to its classes, and from ``parent'' classes to ``child'' classes.
A propagated parameter value can be overridden by specifying that
parameter with a different value at the (child) class.

Example:

\begin{verbatim}
cbq (...,avpkt 1500B,mpu 500B,...) {
    class (...,mpu 600B,...) {
        class (...) ...
    }
}
\end{verbatim}

is equivalent to

\begin{verbatim}
cbq (...,avpkt 1500B,mpu 500B,...) {
    class (...,avpkt 1500B,mpu 600B,...) {
        class (...,avpkt 1500B,mpu 600B,...) ...
    }
}
\end{verbatim}


%------------------------------------------------------------------------------


\section{Class selection paths}
\label{tcngcsp}

Linux Traffic Control supports the creation of configurations where all
classification and metering is done once, but the result is used several
times, e.g. to select classes in subsequent queuing disciplines.

This is done using the \name{dsmark} queuing discipline and the \name{tcindex}
filter. Since their use can be fairly complicated, the \prog{tcng} language
provides a construct that substantially simplifies building such
configurations.

This construct is called a ``class selection path''. Such a path describes
the classes that are selected by a given \name{egress} or \name{dsmark} class.
Class selection paths are used instead of \name{egress} or \name{dsmark}
class numbers, and consist of a
space-separated list of variable names in angle brackets. The classes to
select are later on assigned to these variables (see section \ref{tcnghier}
for more information on assigning classes to variables).

Example:

\begin{verbatim}
egress {
    class (<$c1>) ...;
    ...
    class (<$c2>) ...;
    ...
}
\end{verbatim}

Note that specifying a single class is normally sufficient to uniquely
define the set of classes to use. If more classes along the path are
specified, they are ignored. (Note that only one class per queuing
discipline can be selected in a path, so parent classes in a CBQ
hierarchy must not be specified.)

The classes to select are simply assigned to the corresponding
variables, as shown in the example below:

\begin{verbatim}
egress {
    class (<$c1>)
        if ...;
    class (<$c2>)
        if ...;
    ...
    prio {
        $c1 = class {}
        $c2 = class {}
    }
}
\end{verbatim}

A more interesting example:

\begin{verbatim}
egress {
    class (<$c1>)
        if ...;
    class (<$c2>)
        if ...;
    class (<$c3>)
        if ...;
    ...
    prio {
        prio {
            $c1 = class {}
            $c2 = class {}
        }
        $c3 = class {}
    }
}
\end{verbatim}

If there is no class to select, e.g. because the inner queuing discipline
has no classes, the class selection path is simply left empty, e.g.

\begin{verbatim}
egress {
    class (<>)
        if tcp_dport == 80;
    drop if 1;

    fifo;
}
\end{verbatim}


%------------------------------------------------------------------------------


\section{Processing directives}

A \prog{tcng} configuration file can contain instructions that affect the
overall processing of the file. These instructions can appear wherever
variables or fields are allowed. Unlike variables and fields, they
always affect the global state of the processor (\prog{tcng}).

Currently, one such construct is defined: warnings can be enabled or
disabled using the \name{warn} construct, which consists of the keyword
\name{warn}, followed by a comma-separated list of warning switch names,
like used with the \raw{-W} option, see section \ref{tccusg}.

Example:

\begin{verbatim}
warn "nounused","truncate";
\end{verbatim}

corresponds to an invocation of \prog{tcng} with \raw{-Wnounused -Wtruncate}

Unrecognized warning switches yield a warning. Warning switch names
must be string constants, and cannot be string expressions.

Implementation note: warning switches on the command line take
precedence over warning switches in the configuration file.


%------------------------------------------------------------------------------


\section{Naming conventions}

Names of variables, fields, and macros beginning with two underscores
are reserved for internal use by \prog{tcng} macros. Users should never
use such names for fields or variables defined in their own configurations.

In rare cases, items with such names are part of a published interface
(e.g. the macro \uname{\_\_trTCM\_green}), and may then be used with
due caution.

In order to avoid conflicts with parameter names or other reserved
words of the \prog{tcng} language, field names should be composed of the
protocol name, an underscore, and the name of the actual header field,
e.g. \name{ip\_proto} or \name{tcp\_sport}.


%------------------------------------------------------------------------------


\section{Include files}
\label{tcnginc}

The following sections describe include files containing macros that are
considered to be standard extensions of the tcng language:

\begin{description}
  \item[\name{fields.tc}] definitions of common TCP/IP header fields
   (the actual content of \name{fields.tc} is in the files \name{meta.tc},
   \name{fields4.tc}, and \name{fields6.tc}, which are included by
   \name{fields.tc})
  \item[\name{idiomatic.tc}] provides macros for idiomatic constructs found
    in the context of TCP/IP
  \item[\name{values.tc}] definitions of common values used in TCP/IP headers
  \item[\name{ports.tc}] defines IANA-assigned port numbers
  \item[\name{meters.tc}] macros for implementing traffic meters
\end{description}

Note that \name{meta.tc}, \name{fields4.tc}, \name{fields6.tc},
\name{idiomatic.tc}, and \name{values.tc} are conceptually
part of \name{fields.tc} and should not be included individually.

For convenience, all include files listed above are automatically
included by default. Section \ref{tccusg} describes how this can be
disabled.

\prog{tcng} include files are normally installed in the directory
\url{/usr/lib/tcng/include/}, or, when using a local copy, they are
in \url{./tcng/lib/tcng/include/}.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{TCP/IP header fields}
\label{fieldstc}

The file \name{fields.tc} defines the fields used in the headers of some
of the most common protocols in the TCP/IP protocol suite.%
\footnote{\name{fields.tc} is split into the files \name{meta.tc} for
packet meta data, \name{fields4.tc}
for IPv4 and IP version independent headers, and \name{fields6.tc} for
IPv6 headers. All three are included by \name{fields.tc} and should
not be included individually.}

Note that the same names are also used when constructing packets with
\prog{tcsim}, see section \ref{packetdef}.

In the following sections, we describe header structures as follows:

%%def\\hdr=\\multicolumn{1}{l}{} & \\multicolumn{2}{c}{\\tiny 0} & \\multicolumn{2}{c}{\\tiny 2} & \\multicolumn{2}{c}{\\tiny 4} & \\multicolumn{2}{c}{\\tiny 6} & \\multicolumn{2}{c}{\\tiny 8} & \\multicolumn{2}{c}{\\tiny 10} & \\multicolumn{2}{c}{\\tiny 12} & \\multicolumn{2}{c}{\\tiny 14} & \\multicolumn{2}{c}{\\tiny 16} & \\multicolumn{2}{c}{\\tiny 18} & \\multicolumn{2}{c}{\\tiny 20} & \\multicolumn{2}{c}{\\tiny 22} & \\multicolumn{2}{c}{\\tiny 24} & \\multicolumn{2}{c}{\\tiny 26} & \\multicolumn{2}{c}{\\tiny 28} & \\multicolumn{2}{c}{\\tiny 30}
%%beginskip
\def\hdr{%
  \multicolumn{1}{l}{} &
  \multicolumn{2}{c}{\tiny 0} &
  \multicolumn{2}{c}{\tiny 2} &
  \multicolumn{2}{c}{\tiny 4} &
  \multicolumn{2}{c}{\tiny 6} &
  \multicolumn{2}{c}{\tiny 8} &
  \multicolumn{2}{c}{\tiny 10} &
  \multicolumn{2}{c}{\tiny 12} &
  \multicolumn{2}{c}{\tiny 14} &
  \multicolumn{2}{c}{\tiny 16} &
  \multicolumn{2}{c}{\tiny 18} &
  \multicolumn{2}{c}{\tiny 20} &
  \multicolumn{2}{c}{\tiny 22} &
  \multicolumn{2}{c}{\tiny 24} &
  \multicolumn{2}{c}{\tiny 26} &
  \multicolumn{2}{c}{\tiny 28} &
    \multicolumn{2}{c}{\tiny 30}}
%%endskip
{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{foo\_hdr} &
  \multicolumn{8}{|c|}{Field 1} &
  \multicolumn{8}{c|}{Field 2} &
    \multicolumn{16}{c|}{Field 3} \\
  &
  \multicolumn{8}{c|}{\raw{foo\_field1}} &
  \multicolumn{8}{c|}{\raw{foo\_field2}} &
    \multicolumn{16}{c|}{\raw{foo\_field3}} \\
  \cline{2-33}
  \raw{foo\_}&
  \multicolumn{32}{c|}{Options} \\
  \raw{options} &
  \multicolumn{32}{c|}{$\ldots$} \\
  \cline{2-33}
  \multicolumn{33}{l}{\raw{foo\_data}} \\
\end{tabular}
$$}

\raw{foo\_hdr} marks the beginning of the header of the hypothetical
protocol ``foo''. \raw{foo\_field1} to \raw{foo\_field3} are simply
header fields. If ``foo'' contains an optional part, \raw{foo\_options}
points to it. Finally, if there is data following the ``foo'' header,
\raw{foo\_data} points there. If a ``foo'' header was typically followed
by a header of a protocol called ``bar'', \raw{bar\_hdr} would be
equivalent to \raw{foo\_data}.


% -----


\subsubsection{Packet meta data}

The following meta-data ``fields'' are defined:

\begin{description}
  \item[\name{meta\_protocol}] number of protocol above link layer, e.g.
    \name{ETH\_P\_IP}
  \item[\name{meta\_nfmark}] firewall mark
  \item[\name{meta\_tc\_index}] traffic control index
\end{description}

Note that \name{meta\_nfmark} and \name{meta\_tc\_index} are experimental
and currently serve no purpose.


% -----


\subsubsection{IPv4 header}

The following fields are defined for the IP version 4 header, as described
in \cite{RFC791}:

{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{ip\_hdr} &
  \multicolumn{4}{c|}{Version} &
    \multicolumn{4}{c|}{IHL} &
    \multicolumn{8}{c|}{TOS} &
    \multicolumn{16}{c|}{Total Length} \\
  &
  \multicolumn{4}{c|}{\raw{ip\_v}} &
    \multicolumn{4}{c|}{\raw{ip\_hl}} &
    \multicolumn{8}{c|}{\raw{ip\_tos}} &
    \multicolumn{16}{c|}{\raw{ip\_len}} \\
  \cline{2-33}
  &
  \multicolumn{16}{c|}{Identification} &
    \multicolumn{3}{c|}{Flags} &
    \multicolumn{13}{c|}{Fragment Offset} \\
  &
  \multicolumn{16}{c|}{\raw{ip\_id}} &
    \multicolumn{3}{c|}{(see below)} &
    \multicolumn{13}{c|}{\raw{ip\_off}} \\
  \cline{2-33}
  &
  \multicolumn{8}{c|}{Time to Live} &
    \multicolumn{8}{c|}{Protocol} &
    \multicolumn{16}{c|}{Header Checksum} \\
  &
  \multicolumn{8}{c|}{\raw{ip\_ttl}} &
    \multicolumn{8}{c|}{\raw{ip\_proto}} &
    \multicolumn{16}{c|}{\raw{ip\_sum}} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{Source Address} \\
  &
  \multicolumn{32}{c|}{\raw{ip\_src}} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{Destination Address} \\
  &
  \multicolumn{32}{c|}{\raw{ip\_dst}} \\
  \cline{2-33}
  \raw{ip\_} &
  \multicolumn{32}{c|}{IP options} \\
  \raw{options} &
  \multicolumn{32}{c|}{$\ldots$} \\
  \cline{2-33}
  \multicolumn{33}{l}{\raw{ip\_nexthdr}} \\
\end{tabular}
$$}

The definition of the \name{ip\_tos} octet\footnote{We follow current
practice in calling the second byte of the IPv4 header the ``TOS octet''
and not the ``TOS field'', because the original name ``TOS field''
(\cite{RFC791}) was redefined in now obsolete \cite{RFC1349}.}
revised, and is now called the ``Differentiated Services field''.
The corresponding definitions are described in section \ref{dsfield}.
Additional convenience fields for the traditional and now deprecated
use of \name{ip\_tos} are described in section \ref{tosbits}.

Note that \raw{ip\_src} and \raw{ip\_dst} are of type ``IPv4
address''.

The following flags are used for handling IP fragments:
{\small
$$
\begin{tabular}{|c|c|c|}
  \multicolumn{1}{c}{\tiny 0} &
    \multicolumn{1}{c}{\tiny 1} &
    \multicolumn{1}{c}{\tiny 2} \\
  \hline
  Reserved (0) & Don't Fragment & More Fragments \\
  \raw{ip\_RF} & \raw{ip\_DF} & \raw{ip\_MF} \\
  \hline
\end{tabular}
$$}


% -----


\subsubsection{TCP header}

The following fields are defined for the TCP header, as described
in \cite{RFC793}:

{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{tcp\_hdr} &
  \multicolumn{16}{c|}{Source Port} &
    \multicolumn{16}{c|}{Destination Port} \\
  &
  \multicolumn{16}{c|}{\raw{tcp\_sport}} &
    \multicolumn{16}{c|}{\raw{tcp\_dport}} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{Sequence Number} \\
  &
  \multicolumn{32}{c|}{\raw{tcp\_seq}} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{Acknowledgment Number} \\
  &
  \multicolumn{32}{c|}{\raw{tcp\_ack}} \\
  \cline{2-33}
  &
  \multicolumn{4}{c|}{Offset} &
    \multicolumn{6}{c|}{Reserved} &
    \multicolumn{6}{c|}{Flags} &
    \multicolumn{16}{c|}{Window} \\
  &
  \multicolumn{4}{c|}{\raw{tcp\_off}} &
    \multicolumn{6}{c|}{---} &
    \multicolumn{6}{c|}{(below)} &
    \multicolumn{16}{c|}{\raw{tcp\_win}} \\
  \cline{2-33}
  &
  \multicolumn{16}{c|}{Checksum} &
    \multicolumn{16}{c|}{Urgent Pointer} \\
  &
  \multicolumn{16}{c|}{\raw{tcp\_sum}} &
    \multicolumn{16}{c|}{\raw{tcp\_urp}} \\
  \cline{2-33}
  \raw{tcp\_} &
  \multicolumn{32}{c|}{TCP options} \\
  \raw{options} &
  \multicolumn{32}{c|}{$\ldots$} \\
  \cline{2-33}
  \multicolumn{33}{l}{\raw{tcp\_data}} \\
\end{tabular}
$$}

TCP flags:

{\small
$$
\begin{tabular}{|c|c|c|c|c|c|}
  \multicolumn{1}{c}{\tiny 0} &
    \multicolumn{1}{c}{\tiny 1} &
    \multicolumn{1}{c}{\tiny 2} &
    \multicolumn{1}{c}{\tiny 3} &
    \multicolumn{1}{c}{\tiny 4} &
    \multicolumn{1}{c}{\tiny 5} \\
  \hline
  Urgent & Acknowledgment & Push &
  Reset & Synchronize & Finished \\
  \hline
  \raw{tcp\_URG} & \raw{tcp\_ACK} & \raw{tcp\_PSH} &
  \raw{tcp\_RST} & \raw{tcp\_SYN} & \raw{tcp\_FIN} \\
  \hline
\end{tabular}
$$}

All fields listed above are based on \name{tcp\_hdr}, which in turn
is defined as follows:

\begin{verbatim}
field tcp_hdr = ip_nexthdr if ip_proto == IPPROTO_TCP;
\end{verbatim}

Therefore, accessing any TCP header field automatically adds the
precondition (see section \ref{fields}) \verb"ip_proto == IPPROTO_TCP".

\name{fields.tc} defines a few additional fields for TCP options.
Because \prog{tcng} has currently no mechanism for iterating over a
list of options, these fields are of no practical use and are therefore
not documented.


% -----


\subsubsection{UDP header}

The following fields are defined for the UDP header, as described
in \cite{RFC768}:

{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{udp\_hdr} &
  \multicolumn{16}{c|}{Source Port} &
    \multicolumn{16}{c|}{Destination Port} \\
  &
  \multicolumn{16}{c|}{\raw{udp\_sport}} &
    \multicolumn{16}{c|}{\raw{udp\_dport}} \\
  \cline{2-33}
  &
  \multicolumn{16}{c|}{Length} &
    \multicolumn{16}{c|}{Checksum} \\
  &
  \multicolumn{16}{c|}{\raw{udp\_ulen}} &
    \multicolumn{16}{c|}{\raw{udp\_sum}} \\
  \cline{2-33}
  \multicolumn{33}{l}{\raw{udp\_data}} \\
\end{tabular}
$$}

All fields listed above are based on \name{udp\_hdr}, which in turn
is defined as follows:

\begin{verbatim}
field udp_hdr = ip_nexthdr if ip_proto == IPPROTO_UDP;
\end{verbatim}


% -----


\subsubsection{ICMP header}

The following fields are defined for the ICMP header, as described
in \cite{RFC792}:

{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{icmp\_hdr} &
  \multicolumn{8}{c|}{Type} &
  \multicolumn{8}{c|}{Code} &
    \multicolumn{16}{c|}{Checksum} \\
  &
  \multicolumn{8}{c|}{\raw{icmp\_type}} &
  \multicolumn{8}{c|}{\raw{icmp\_code}} &
    \multicolumn{16}{c|}{\raw{icmp\_sum}} \\
  \cline{2-33}
  \multicolumn{33}{l}{\raw{icmp\_data}} \\
\end{tabular}
$$}

All fields listed above are based on \name{icmp\_hdr}, which in turn
is defined as follows:

\begin{verbatim}
field icmp_hdr = ip_nexthdr if ip_proto == IPPROTO_ICMP;
\end{verbatim}


% -----


\subsubsection{IGMP header}

The following fields are defined for the IGMP header, as described
in \cite{RFC2236}:

{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{igmp\_hdr} &
  \multicolumn{8}{c|}{Type} &
  \multicolumn{8}{c|}{Max Resp Time} &
    \multicolumn{16}{c|}{Checksum} \\
  &
  \multicolumn{8}{c|}{\raw{igmp\_type}} &
  \multicolumn{8}{c|}{\raw{igmp\_code}} &
    \multicolumn{16}{c|}{\raw{igmp\_sum}} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{Group Address} \\
  &
  \multicolumn{32}{c|}{\raw{igmp\_group}} \\
  \cline{2-33}
\end{tabular}
$$}

Because field names generally follow the corresponding naming conventions
used in BSD \path{/usr/include/netinet/} include files, the
``Max Response Time'' field is called \raw{igmp\_code}
(for ``routing code'').

Note that \raw{igmp\_group} is of type ``IPv4 address''.

All fields listed above are based on \name{igmp\_hdr}, which in turn
is defined as follows:

\begin{verbatim}
field igmp_hdr = ip_nexthdr if ip_proto == IPPROTO_IGMP;
\end{verbatim}


% -----


\subsubsection{IPv6 header}

The following fields are defined for the IP version 6 header, as described
in \cite{RFC2460}:

{\small
$$
\begin{tabular}{l|cccccccccccccccccccccccccccccccc|}
  \hdr \\
  \cline{2-33}
  \raw{ip6\_hdr} &
  \multicolumn{4}{c|}{Version} &
    \multicolumn{8}{c|}{Traffic Class} &
    \multicolumn{20}{c|}{Flow Label} \\
  &
  \multicolumn{4}{c|}{\raw{ip6\_v}} &
    \multicolumn{8}{c|}{\raw{ip6\_tc}} &
    \multicolumn{20}{c|}{\raw{ip6\_flow}} \\
  \cline{2-33}
  &
  \multicolumn{16}{c|}{Payload Length} &
    \multicolumn{8}{c|}{Next Header} &
    \multicolumn{8}{c|}{Hop Limit} \\
  &
  \multicolumn{16}{c|}{\raw{ip6\_plen}} &
    \multicolumn{8}{c|}{\raw{ip6\_nxt}} &
    \multicolumn{8}{c|}{\raw{ip6\_hlim}} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{} \\
  &
  \multicolumn{32}{c|}{Source Address} \\
  &
  \multicolumn{32}{c|}{(16 bytes)} \\
  &
  \multicolumn{32}{c|}{} \\
  &
  \multicolumn{32}{c|}{\raw{ip6\_src}} \\
  &
  \multicolumn{32}{c|}{} \\
  \cline{2-33}
  &
  \multicolumn{32}{c|}{} \\
  &
  \multicolumn{32}{c|}{Destination Address} \\
  &
  \multicolumn{32}{c|}{(16 bytes)} \\
  &
  \multicolumn{32}{c|}{} \\
  &
  \multicolumn{32}{c|}{\raw{ip6\_dst}} \\
  &
  \multicolumn{32}{c|}{} \\
  \cline{2-33}
  \multicolumn{33}{l}{\raw{ip6\_nexthdr}} \\
\end{tabular}
$$}

Note that \raw{ip6\_src} and \raw{ip6\_nexthdr} are of type ``IPv6
address''.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Idiomatic constructs}

\name{idiomatic.tc} defines a few macros for common expressions in
classification.

Those macros which test for specific properties are optimized for
rapid processing in \path{tcng} and typically also yield more
efficient classifiers. For each such macro, there is also a macro that
tests for the absence of that property. Those macros have the
same name, prefixed with \name{not\_}, and are normally more
efficient than using the negation operator (\verb"!").

Note that \name{idiomatic.tc} is included via
\name{fields.tc} and should not be included directly.


% -----


\subsubsection{Fragments}

\name{ip\_is\_fragment} is non-zero (``true'') if the packet is an IP
fragment, i.e. if the ``more fragments'' flag (\name{ip\_MF}) is set
or the fragment offset (\name{ip\_off}) is non-zero.

The macro \name{not\_ip\_is\_fragment} is the negation of
\name{ip\_is\_fragment}.


Example:

\begin{verbatim}
#include "fields.tc"

...
    drop if ip_is_fragment;
\end{verbatim}


% -----


\subsubsection{Tiny fragments}

\name{ip\_is\_tiny\_fragment} is non-zero if the packet is a
formally correct yet anomalous fragment that is called a ``tiny
fragment'' (further details can be found in \cite{RFC1858} and
\cite{RFC3128}). Tiny fragments are sometimes abused to bypass
firewalls using classification by transport layer headers.

The macro \name{not\_ip\_is\_tiny\_fragment} is the negation of
\name{ip\_is\_tiny\_fragment}.

Note that the test for tiny fragments is typically an expensive
operation, so performance degradation may occur when including
\name{ip\_is\_tiny\_fragment} or \name{not\_ip\_is\_tiny\_fragment}
in a classifier.


% -----


\subsubsection{IP options}

\name{ip\_has\_options}	is non-zero if the packet has IP options,
i.e. if the IPv4 header length (\name{ip\_hl}) is different from five.

The macro \name{not\_ip\_has\_options} is the negation of
\name{ip\_has\_options}.


% -----


\subsubsection{TCP connections}

\name{tcp\_incoming} is non-zero if the packet is the SYN segment of
an incoming TCP connection. Note that ``incoming'' is from the
perspective of the classifier, i.e. \name{tcp\_incoming} is non-zero if
the connection is being set up in the same direction in which the
current packet is traveling, as shown in figure \ref{tcpincoming}.

\begin{figure}[htb]
\begin{center}
\epsfig{file=incoming.eps}
\end{center}
\caption{Result of \name{tcp\_incoming}.}
\label{tcpincoming}
\end{figure}

The macro \name{not\_tcp\_incoming} is the negation of
\name{tcp\_incoming}.


% -----


\subsubsection{Retrieving the Differentiated Services Field}
\label{dsfield}

The DS field defined in \cite{RFC2474} is frequently used in
classification on networks implementing the Differentiated Services
architecture (\cite{RFC2475}).

\name{idiomatic.tc} defines the following fields:

\name{ip\_dsfield} is the same as \name{ip\_tos} (see section
\ref{fieldstc}).

\name{ip\_dscp} are the upper six bits of \name{ip\_dsfield}, i.e. the
DS field without the ECN bits (\cite{RFC3168}).

Example:

\begin{verbatim}
#include "fields.tc"

...
    class (<$ef>)
        if ip_dscp == 0b101110; /* RFC2598 */
\end{verbatim}


% -----


\subsubsection{Content of the Type of Service octet}
\label{tosbits}

Although obsoleted by \cite{RFC2474}, traditional use of the Type of
Service octet as described in \cite{RFC791} is still common.

\name{idiomatic.tc} defines the following fields:

\name{ip\_tos\_prec} are the upper three bits of \name{ip\_tos}.

\name{ip\_tos\_delay} is the bit that, if set, indicates the desire
for short delay.

\name{ip\_tos\_throughput} is the bit that, if set, indicates the desire
for high throughput.

\name{ip\_tos\_reliability} is the bit that, if set, indicates the desire
for high reliability.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Values used in TCP/IP headers}
\label{valuestc}

\name{values.tc} contains macros for common values in meta data and in
TCP/IP header fields. The following groups of values are defined:

\begin{description}
  \item[\name{ETH\_P\_\meta{protocol}}] numbers of protocols above the link
    layer, e.g. \name{ETH\_P\_IP}
  \item[\name{IPPROTO\_\meta{protocol}}] transport layer protocol numbers,
    e.g. \name{IPPROTO\_UDP}
  \item[\name{TCPOPT\_\meta{kind}}] type of TCP option
  \item[\name{ICMP\_\meta{type}}] ICMP message type, e.g.
    \name{ICMP\_TIMXCEED}
  \item[\name{ICMP\_\meta{code}}] ICMP message type sub code,
    e.g. \name{ICMP\_TIMXCEED\_INTRANS}
  \item[\name{IGMP\_\meta{type}}] IGMP message type, e.g.
    \name{IGMP\_MEMBERSHIP\_QUERY}
\end{description}

The names are the same as used
in BSD \path{/usr/include/netinet/} include files.
Please see \name{values.tc} for the complete list of definitions.

Examples:

\begin{verbatim}
... if meta_protocol == ETH_P_IP;
... if ip_proto == IPPROTO_TCP;
... if icmp_type == ICMP_UNREACH &&
      icmp_code == ICMP_UNREACH_PORT;
... if igmp_type == IGMP_V2_LEAVE_GROUP;
\end{verbatim}

Note that \name{values.tc} is included via
\name{fields.tc} and should not be included directly.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Ports}
\label{portstc}

The include file \name{ports.tc} defines macros for almost all TCP and UDP
port names and numbers registered by IANA.%
\footnote{\url{http://www.iana.org/assignments/port-numbers}}

The names of port macros begin with \raw{PORT\_}, followed by the port
name in upper case. Special characters (e.g. hyphens, asterisks, or
dots) in the port name are converted to underscores. As a special case
for \verb"whois++", \verb"++" becomes \name{\_PP}.

When the IANA port numbers database contains multiple port numbers for
the same name, the entry with the highest number is used. If an entry
defines a range, the lower bound of the range is used.

Examples:

\begin{verbatim}
#define PORT_HTTP 80            /* http */
#define PORT_HTTP_ALT 8080      /* http-alt */
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Meters}
\label{meterstc}

\name{meters.tc} defines macros to implement the following common traffic
metering functions:

\begin{description}
  \item[\name{SLB}] a single leaky bucket meter
  \item[\name{DLB}] a double leaky bucket meter
  \item[\name{srTCM}] a single rate three color meter \cite{RFC2697}
  \item[\name{trTCM}] a two rate three color meter \cite{RFC2698}
\end{description}


% -----


\subsubsection{Single leaky bucket meter}

The single leaky bucket meter is the most basic meter:
``tokens'' enter the bucket at rate \name{cir},
until the bucket reaches the size \name{cbs}. At this point, further
tokens are discarded. A packet conforms to the meter if the number
of tokens in the bucket corresponds to at least the size of the
packet. In this case, the respective number of tokens is subtracted
from the bucket, and the packet has passed the conformance test.
Otherwise, the packet has failed the test, and no tokens are
subtracted.

\begin{figure}[htb]
\begin{center}
\epsfig{file=slb.eps}
\end{center}
\caption{Single leaky bucket meter.}
\label{slbfig}
\end{figure}

We illustrate the operation of a single leaky bucket meter in figure
\ref{slbfig}, by using the analogy of a bucket that contains water,
and where a faucet is briefly opened whenever a packet is accepted,
to let go of the corresponding quantity of water.

Meters are generated by a macro whose name is the abbreviated
meter name, e.g. \name{SLB}. Typically, the meter
is assigned to a variable, e.g.

\begin{verbatim}
$slb = SLB(cir 100kbps,cbs 10kB);
\end{verbatim}

\name{SLB} takes the following parameters:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{cbs} & Bytes & Committed burst size \\
  \name{cir} & bps   & Committed information rate \\
  \name{mpu} & Bytes & Minimum policed unit \\
\end{tabular}
\end{center}

Only \name{mpu} is optional, all other parameters are required.

There are two functions for testing a single or double bucket meter:
\name{\meta{meter}\_ok} tests whether the meter has detected conformance
or not, and returns the corresponding truth value. Example:

\begin{verbatim}
#include "fields.tc"
#include "meters.tc"

$foo_meter = SLB(cir 50kbps,cbs 20kB);
...
    class (<$foo_class>)
        if ip_src == host "foo.mydomain.net" &&
          SLB_ok($foo_meter);
\end{verbatim}

\name{\meta{meter}\_else\_drop} performs the same test, but drops the
packet if detecting non-conformance.

Example:

\begin{verbatim}
#include "fields.tc"
#include "meters.tc"

$icmp_meter = SLB(cir 10kbps,cbs 20kB,pir 100kbps,pbs 5kB);
$icmp_meter = SLB(cir 10kbps,cbs 20kB);
...
    class (<$icmp_class>)
        if ip_proto == IPPROTO_ICMP &&
          SLB_else_drop($icmp_meter);
\end{verbatim}

Note that terms of equal precedence in an expressions are evaluated from
left to right, so that

\begin{verbatim}
    ... if tcp_sport == 80 && SLB_ok($x);
\end{verbatim}

and

\begin{verbatim}
    ... if SLB_ok($x) && tcp_sport == 80;
\end{verbatim}

are not equivalent. In the latter expression, the conformance test is
always performed when reaching this \name{if} condition, and if
successful, the corresponding tokens are removed from the bucket,
even if the following test of \name{tcp\_sport} fails.


% -----


\subsubsection{Double leaky bucket meter}

The double leaky bucket meter, shown in figure \ref{dlbfig}, is similar
to the single leaky bucket meter.
The only differences are that tokens must be simultaneously available in two
distinct buckets,
and that tokens are removed from both buckets if the packet is
found to be conforming.

\begin{figure}[htb]
\begin{center}
\epsfig{file=dlb.eps}
\end{center}
\caption{Double leaky bucket meter.}
\label{dlbfig}
\end{figure}

\name{DLB} has the following parameters:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{cbs} & Bytes & Committed burst size \\
  \name{cir} & bps   & Committed information rate \\
  \name{mpu} & Bytes & Minimum policed unit \\
  \name{pbs} & Bytes & Peak burst size \\
  \name{pir} & bps   & Peak information rate \\
\end{tabular}
\end{center}

Only \name{mpu} is optional, all other parameters are required.

The functions for testing a double leaky bucket meter are described
in the previous section.


% -----


\subsubsection{Single rate three color meter}
\label{srtcm}

The single rate three color meter is typically used for implementing the
corresponding three color marker defined in \cite{RFC2697}.
Its configuration is shown in figure \ref{srtcmfig}.

\begin{figure}[htb]
\begin{center}
\epsfig{file=srtcm.eps}
\end{center}
\caption{Single rate three color meter.}
\label{srtcmfig}
\end{figure}

Also this meter is created with a macro whose names corresponds to the
meter's acronym, e.g.

\begin{verbatim}
$tcm = srTCM(cir 1Mbps,cbs 50kB,ebs 40kB);
\end{verbatim}

\name{srTCM} has the following parameters:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{cbs} & Bytes & Committed burst size \\
  \name{cir} & bps   & Committed information rate \\
  \name{ebs} & Bytes & Extended burst size \\
  \name{mpu} & Bytes & Minimum policed unit \\
\end{tabular}
\end{center}

Only \name{mpu} is optional, all other parameters are required.

The color assigned by a three color meter is tested with macros
\name{\meta{meter}\_\meta{color}}, where \meta{color} is one of
\name{green}, \name{yellow}, and \name{red}.

Example:

\begin{verbatim}
#include "fields.tc"
#include "meters.tc"

$m = srTCM(cir 1Mbps,cbs 100kB,ebs 40kB);
...
    class if ip_dst:8 == 10.0.0.0 && srTCM_green($m);
    class if ip_dst:8 == 10.0.0.0 && srTCM_yellow($m);
    class if ip_dst:8 == 10.0.0.0 && srTCM_red($m);
...
\end{verbatim}


% -----


\subsubsection{Two rate three color meter}
\label{trtcm}

The two rate three color meter shown in figure \ref{trtcmfig} is typically
used for implementing the corresponding three color marker defined in
\cite{RFC2698}.

\begin{figure}[htb]
\begin{center}
\epsfig{file=trtcm.eps}
\end{center}
\caption{Two rate three color meter.}
\label{trtcmfig}
\end{figure}

\name{trTCM} uses the same parameters as \name{DLB}, namely:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{cbs} & Bytes & Committed burst size \\
  \name{cir} & bps   & Committed information rate \\
  \name{mpu} & Bytes & Minimum policed unit \\
  \name{pbs} & Bytes & Peak burst size \\
  \name{pir} & bps   & Peak information rate \\
\end{tabular}
\end{center}

Only \name{mpu} is optional, all other parameters are required.

The functions for testing a two rate three color meter are described
in the previous section.


% -----



\subsubsection{Accelerating three color meters}
\label{acctcm}

Color test macros are frequently used as shown in the example in
section \ref{srtcm}, e.g.

\begin{itemize}
  \item the three tests are made in adjacent \name{if} clauses
  \item the colors are tested in the order \name{green}, \name{yellow},
    and \name{red}
  \item all three tests have the same static precondition (in the
    example, this is \verb"ip_dst:8 == 10.0.0.0")
\end{itemize}

In this case, an alternative set of macros can be used, which exploits
this regularity, and which is easier to process for \name{tcng}. These
macros have the same names as the regular color test macros, but a double
underscore (\name{\_\_}) is added before these names.

Example:

\begin{verbatim}
#include "fields.tc"
#include "meters.tc"

$m = trTCM(cir 1Mbps,cbs 100kB,pir 10Mbps,pbs 40kB);
...
    class if ip_dst:8 == 10.0.0.0 && __trTCM_green($m);
    class if ip_dst:8 == 10.0.0.0 && __trTCM_yellow($m);
    class if ip_dst:8 == 10.0.0.0 && __trTCM_red($m);
...
\end{verbatim}

Note that these macros will produce incorrect results if the constrains
listed above are not followed. Also, the accelerated macros cannot be
mixed with their non-accelerated counterparts.


%==============================================================================


\chapter{Under the hood}
\label{chphood}

A number of constructs of the \prog{tcng} language are mainly used
when implementing new metering mechanisms, or when adding support
for features of a specific target.


%------------------------------------------------------------------------------


\section{Field roots}

To access information that is not contained in packet data, e.g.
meta-information generated by the kernel, other roots than \name{raw} can
be used with the \name{field\_root} construct:

\begin{verbatim}
field my_meta_data = field_root(10);
field foo = my_meta_data[10].ns;
\end{verbatim}

Field roots are numbered, and each number corresponds to a
separate address space, in which fields can be defined by offset.
Field root numbers below ten are reserved for internal or future use
by tcng and cannot be accessed with the \name{field\_root} construct.

Currently, the following field root numbers are assigned:
\begin{tabular}{rl}
  Number & Description \\
  0 & Packet content \\
  1 & Packet meta-data \\
\end{tabular}

Implementation note: field roots above one are currently only supported
at the ``external'' interface. Field root numbers correspond to offset
group numbers. Note that \prog{tcng} auto-assigns offset group numbers starting
with 100, so user-defined field roots should be in the range 10 to 99.


%------------------------------------------------------------------------------


\section{Buckets}

The metering macros for \name{if} constructs described in section
\ref{meterstc} are based on ``leaky buckets'', sometimes also called
``token buckets''.

In an \name{if} construct, buckets are used by 
including metering primitives in the if expression. This
approach gives enormous flexibility, allowing the definition of all
common metering schemes with a small set of primitives.

Buckets have the following parameters:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{burst} & Bytes & bucket depth \\
  \name{mpu}   & Bytes & minimum policed unit \\
  \name{rate}  & bps   & bucket fill rate \\
\end{tabular}
\end{center}

Only \name{mpu} is optional, all other parameters are required.

In addition, buckets can have tags (see section \ref{tags})
and pragmas (see section \ref{pragmas}).


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Metering primitives}

The following metering primitives are available:

\begin{description}
  \item[\name{conform \meta{bucket}}] if the token bucket 
    contains enough tokens for the current packet,
    \name{conform} returns 1, otherwise it returns 0.
  \item[\name{count \meta{bucket}}] subtracts tokens for the current packet
    from the specified token bucket. If not enough
    tokens are available, \name{count} sets the bucket size to zero.
    \name{count} always returns 1.
\end{description}

The metering primitives can be used like any Boolean expression.

Examples:

\begin{verbatim}
// Typical two-rate meter

$p1 = bucket(rate 1Mbps,burst 20kB);
$p2 = bucket(rate 2Mbps,burst 2kB);

prio {
    class if conform $p1 && conform $p2 && count $p1 && count $p2;
    class if conform $p2 && count $p2;
    class if 1;
}
\end{verbatim}

\begin{verbatim}
// Rate limiter

$p = bucket(rate 50kbps,burst 3kB,mpu 200B);

ingress {
    class (1) if conform $p && count $p;
    drop if 1;
}
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Implementation limitations}

There are currently the following restrictions on the use of metering
primitives:

\begin{itemize}
 \item due to limitations of the expression manipulation functions in
   \prog{tcng},
   only comparably simple expressions can be processed (\prog{tcng} may run out
   of memory when trying to process complex expressions)
 \item new tokens are only added when encountering \name{conform}
\end{itemize}


%------------------------------------------------------------------------------


\section{Policing primitives}

An \name{if} construct can reach a decision (such as to attribute a
packet to a specific class or to drop it) in two ways: either by
returning ``true'' (i.e. a non-zero value), in which case the
corresponding class or drop action is taken, or by encountering a
so-called policing primitive while evaluating the classification
expression.

The following policing primitives are available:

\begin{description}
  \item[\raw{drop}] indicates to the queuing discipline that the
    packet be dropped and terminates further
    evaluation of the if expression
  \item[\raw{reclassify}] accepts the packet, but indicates to the
    queuing discipline that the packet be reclassified.
\end{description}

The policing primitives can be used like any Boolean expression.

Examples:

\begin{verbatim}
// The rate limiter from the previous example

$p = bucket(rate 50kbps,burst 3kB,mpu 200B);

ingress {
    class (1) if (conform $p && count $p) || drop;
}
\end{verbatim}


%------------------------------------------------------------------------------


\section{Advanced variables: structures}

When using ``structures'', a single variable can also contain multiple
values. This concept is very similar to C.

A structure contains so-called ``entries''. Each entry has a name, and
can contain any value that may be stored in a variable. Structure entries
are created or changes by assigning to a variable which is followed by a
dot (\verb".") and the entry name. Likewise, structure entries are read
by appending a dot and the entry name to the variable name.

Example:

\begin{verbatim}
$a.x = 6;
$a.y = 7
$a.z = $a.x*$a.y;
\end{verbatim}

The value of a structure entry may also be another structure, whose entries
are then accessed by adding more dots and entry names.

Example:

\begin{verbatim}
$a = 1;
$a.b = 2;
$a.b.c = 4;
$a.b.c.d = $a+$a.b+$a.b.c;
\end{verbatim}

Note that assigning a structure copies all fields, and that overwriting
a structure value removes the whole structure.

Examples:

\begin{verbatim}
$a.x = 1;
$a.y = "foo";
$b = $a;  // now $b.x is 1, and $b.y is "foo"
$a = 13;  // this also eliminates $a.x and $a.y
\end{verbatim}

Also note that structures are treated like single variable values, so scoping
applies to the whole structure, but not to individual structure entries.

Example:

\begin{verbatim}
$x.y = 13;
{
    $x.y = 7;
}
\end{verbatim}

At the end, \verb"$x.y" has the value 7, not 13.

Note that structure entries cannot be forward-referenced, because they
are part of the value of the corresponding variable, and not variables
by themselves.

Also note that in \prog{tcng}, ``.'' is treated as a part of the
variable name, and not as an operator. Therefore, constructs like
\verb"($var).flt" are not allowed, and yield a syntax error. This
restriction may be lifted in the future.


%------------------------------------------------------------------------------


\section{Advanced variables: compound expressions}
\label{advcomp}

Particularly when constructing structures in macros, it is necessary to
store intermediate results in variables, before producing the final
result. E.g.

\begin{verbatim}
#define MACRO(result) \
  $tmp.a = 10; \
  $tmp.b = 20; \
  result = $tmp;
...
MACRO($var);
\end{verbatim}

Unfortunately, ``call by reference''\footnote{This is actually closer to the
even more exotic ``call by name'', e.g. also {\tt MACRO(field sth)} would work
in this example.} is a concept that is not used anywhere else in the
\prog{tcng} language, and it would be more natural if the macro could
simply return a value, so one could use it as follows:

\begin{verbatim}
$var = MACRO;
\end{verbatim}

This is possible with the compound expressions. A compound expression
consists of zero or more variable or field assignments, followed by
a single expression. All this is enclosed by curly braces.

Example:

\begin{verbatim}
#define MACRO \
  { $tmp.a = 10; \
    $tmp.b = 20; \
    $tmp; }
\end{verbatim}

The usual scoping rules related to curly braces apply.

C programmers using \prog{gcc}'s statement expression extension may
wish to put compound expressions in parentheses, which yields virtually
the same syntax.


%------------------------------------------------------------------------------


\section{The \name{precond} function}
\label{precond}

In some cases, it may be desirable to have more control over how
preconditions are used. For example, in a firewall, one may wish to
ignore expressions entirely if they access any unavailable fields,
independent of how these fields are used in the expression.

The \name{precond} function allows to do this: it returns the combined
preconditions of all accesses of its argument. Note that
preconditions encountered in preconditions are evaluated in the
normal way.

Example:

\begin{verbatim}
drop if !precond(ip_src != 1.2.3.4 || tcp_sport != 80);
drop if ip_src != 1.2.3.4 || tcp_sport != 80;
\end{verbatim}

The first rule drops non-TCP packets even if their source IP
address is 1.2.3.4.


%------------------------------------------------------------------------------


\section{Hierarchical structure}
\label{tcnghier}

Traffic control elements are attached to certain points in the hierarchy
dangling off an interface. We call such a point of attachment the ``parent''
of an element. The parent is usually given by the nesting of elements.

Example:

\begin{verbatim}
eth0                                # ID            # Parent
cbq (1,...) {                       1:0             root
    class (1,...) {                 1:1             1:0
        class (3,...) {             1:3             1:1
        }
        prio (2) {                  2:0             1:1
            class (1) ...           2:1             2:0
        }
    }
    class (2,...) {                 1:2             1:0
        tcindex (1,...) {           prio 1 at 1:2   1:2
            class (4,...)           1:4             1:2
                on (...);           ?               prio 1 at 1:2
        }
    }
}
\end{verbatim}

Sometimes, this is not sufficient to obtain the desired structure. In such
cases, variables can be used to specify elements at other locations.
Queuing disciplines, classes, and filters can be assigned to variables by
putting a variable assignment in front of their specification, e.g

\begin{verbatim}
$q = prio;
\end{verbatim}

The parent of this element is then determined by the location of its
specification, not the location of its actual use. To access such an
element, use the keyword \name{qdisc}, \name{class}, or \name{filter},
followed by the variable name.

Similarly, a policer or bucket may be used at several places, and can
therefore also be stored in a variable.

Examples:

\begin{verbatim}
// Single queuing discipline shared by multiple classes
// Note: in the case of "prio", this is actually pointless.

prio { 
    $q = tbf ...;
    class (1) {
        qdisc $q;
    }
    class (2) {
        qdisc $q;
    }
}
\end{verbatim}

\begin{verbatim}
// Top-level filter directly selects classes deeper in the tree

cbq ... {
    $f = rsvp ...;
    class ...
        on filter $f element (...)
    {
        class ...
            on filter $f element (...);
        class ...
            on filter $f element (...);
    }
}
\end{verbatim}

Note: \raw{on filter \$f element ($\ldots$)} can be abbreviated to
\raw{on \$f($\ldots$)}.

\begin{verbatim}
// Different filters select the same class

$c = class ...
tcindex (...) {
    class $c
        on ...;
}
rsvp ... {
    class $c
        on ...;
}
\end{verbatim}

\begin{verbatim}
// Policer used for multiple flows

$p = police ...;
prio ... {
    class ...
        ... police $p;
    class ...
        ... police $p;
    class ...
}
\end{verbatim}

When using a bucket variable in an \name{if} expression, the
\name{bucket} (or \name{police}, for compatibility) keyword
can be omitted, e.g. \raw{conform bucket \$p} is equivalent to
\raw{conform \$p}.


%------------------------------------------------------------------------------


\section{Queuing disciplines without classes}

A queuing discipline that is specified directly inside another queuing
discipline (e.g. \name{ingress} or \name{egress}), without a class
between them, implies creation of an unnumbered, parameter-less class,
which then contains that queuing discipline.

\begin{verbatim}
egress {
    red ...;
}
\end{verbatim}

is equivalent to

\begin{verbatim}
egress {
    class { red ... }
}
\end{verbatim}

Note that the latter form is now deprecated.


%------------------------------------------------------------------------------


\section{Class selection path internals}

As mentioned in section \ref{tcngcsp}, class selection paths are
equivalent to a mixture of \name{dsmark} and \name{tcindex}. We examine
the structure generated for the two class selection path examples.

In the first example, a \name{tcindex} filter is automatically added to the
\name{prio}
queuing discipline, one filter element is created for each \name{prio} class,
and the \name{dsmark} class numbers are set accordingly. It is equivalent to
the following configuration:

\begin{verbatim}
dsmark {
    class (1)
        if ...;
    class (2)
        if ...;
    ...
    prio {
        tcindex (mask 3) {
            $c1 = class
                on (1);
            $c2 = class
                on (2);
        }
    }
}
\end{verbatim}

And this is the the equivalent configuration for the second example, without
using class selection paths:

\begin{verbatim}
dsmark {
    class (5)
        if ...;
    class (6)
        if ...;
    class (8)
        if ...;
    ...
    prio {
        tcindex (mask 0xc0,shift 2) {
            class
                on (1)
            {
                prio {
                    tcindex (mask 3) {
                        class
                            on (1);
                        class
                            on (2);
                    }
                }
            }
            class
                on (2);
        }
    }
}
\end{verbatim}

Implementation note: If multiple levels of queuing disciplines need to be
traversed, \prog{tcng} tries to use a certain set of bits in the
\name{tcindex} value at
each queuing discipline. If the number of available bits is exceeded,
\prog{tcng}
falls back to assigning a distinct value for each path. This may affect
space and time efficiency of the resulting configuration.

Also note that class selection paths cannot be nested, e.g.

\begin{verbatim}
dsmark {
    class (<$x>) if ...;
    ...
        dsmark { 
            $y = class (<$x>);
            ...
                $y = class ...;
            ...
        }
    ...
}
\end{verbatim}

is not supported.


%------------------------------------------------------------------------------


\section{Tags}
\label{tags}

Queuing disciplines, classes, filters, tunnels, filter elements (except
elements of \name{route} and \name{if}), policers, and buckets can be
numbered.

Similarly, a text string can be assigned to all those items, and also to
devices. This ``tag''
is shown in the location map (see section \ref{locfile}). It can be assigned
by simply putting a string expression
among the parameters of the respective item.
Optionally, this string can be prefixed with the parameter name \name{tag},
which is useful when writing macros.

Example:

\begin{verbatim}
dev "eth0"(tag "blah") {
    fifo (limit 100kB,"my_fifo");
}
\end{verbatim}

Note that tags may not contain spaces.


%------------------------------------------------------------------------------


\section{Pragmas}
\label{pragmas}

The \prog{tcng} language allows implementation-specific extensions via the
\name{pragma} construct or parameter.

Pragmas can be added with the \name{pragma} parameter to devices, queuing
disciplines, classes, filters, tunnels, filter elements, and policers.
The \name{pragma} parameter is followed by a space-separated list of
strings, each containing one or more non-blank printable characters.

Examples:

\begin{verbatim}
fifo (pragma "head-drop");
\end{verbatim}

\begin{verbatim}
eth0 (pragma "rate=2Mbps" "mode=duplex")
\end{verbatim}

Furthermore, a global pragma can be specified before the first
device or queuing discipline with the \name{pragma} construct:

Example:

\begin{verbatim}
pragma ("debug=all");
\end{verbatim}

The \prog{tcng} language does not define the semantics of pragmas. An
implementation may choose to ignore or refuse unrecognized pragmas,
or pragmas in general, or it may fail in obscure ways in the
presence of pragmas.


%==============================================================================


\chapter{Historical constructs}
\label{chphist}

The \prog{tcng} language was originally designed to provide a clean and
consistent way for describing the configuration of traffic control
elements in the Linux kernel. Since then,
\prog{tcng} has been extended to allow -- mainly for classification -- much
more abstraction from the underlying implementation, than originally
planned.

So far, we have discussed the ``modern'' part of the \prog{tcng} language,
which gives access to all commonly used functionality, and which provides
an abstract model of the classification process.

In the following sections, we will describe alternative means for
configuring traffic control mechanisms, and syntactical constructs which
are supported for backward compatibility.


%------------------------------------------------------------------------------


\section{Interfaces revisited}
\label{inthist}

\prog{tcng} supports various abbreviated forms of interface sections.
All these abbreviations have in common that they either cause some
restrictions, or may lead to ambiguities.
The following abbreviations are possible:

\begin{itemize}
  \item the keyword \name{dev} can be omitted, in which case the
    interface name must be a bare word or in double quotes, i.e. it
    is not possible to use an arbitrary string expression without
    the \name{dev} keyword.
  \item if not only \name{dev}, but also the name is omitted,
    the default interface \name{eth0} (defined in \path{config.h}) or
    the default set with the command-line option \raw{-i} is used.
  \item if omitting the interface name, also the curly braces around an
    interface section can be omitted,
    but this can lead to confusing ambiguities in variable and field
    definition scoping.
\end{itemize}

Examples:

\begin{verbatim}
/* no interface name, no curly braces */
fifo;
\end{verbatim}

\begin{verbatim}
/* no interface name */
{
    fifo;
}
\end{verbatim}

\begin{verbatim}
/* no "dev" keyword, name is bare word */
eth0 {
    fifo;
}
\end{verbatim}

\begin{verbatim}
/* name contains special characters and must
   be quoted */
"funny:name" {
    fifo;
}
\end{verbatim}

Interface names can also be followed by a parameter list in parentheses.
This is necessary when using tags or pragmas, see sections \ref{tags} and
\ref{pragmas}.


%------------------------------------------------------------------------------


\section{Queuing discipline numbers}

Like most other traffic control elements, also queuing disciplines are
numbered. The number of a queuing discipline is usually automatically
assigned, but it can also be explicitly set by including it among the
parameters, e.g.

\begin{verbatim}
prio (2) {
    ...
}
\end{verbatim}


%------------------------------------------------------------------------------


\section{Filters}

When using the \name{tc} target (see section \ref{targets}), \prog{tcng}
can also configure filters directly, without using the \name{if} construct.
Note that it is currently not possible to combine filters and \name{if}
at the same queuing discipline

A filter consists of the filter and of its elements, just like a queuing
discipline
consists of the queuing discipline and its classes. With some filters,
elements are optional.

A filter specification begins with the filter name, followed by an
optional parameter list (in parentheses), and the optional filter body, in
curly braces. The filter body contains classes.

Example:

\begin{verbatim}
prio {
    rsvp (ipproto "tcp") {
        class (1)
            on (dst 10.0.0.1);
        class (2) 
            on (dst 10.0.0.2);
    }
}
\end{verbatim}


The priority of a filter is usually automatically assigned in the order in
which filter specifications appear in the configuration. It can be
explicitly set by including the number among the parameters. Automatic
assignment takes manually assigned priorities into account and continues
with the next available number. Priority numbers must always increase.
Example:

\begin{verbatim}
... tcindex(3) ...
\end{verbatim}


%------------------------------------------------------------------------------


\section{Filter elements}
\label{filtelem}

Classes are selected by filter elements in a way similar to \name{if}
expressions. We call \name{if} and \name{on} constructs ``selectors''.

Selectors
are placed between the parameters of a class and its body. Filter elements
begin with the keyword \name{on}, followed by the optional list of filter
element parameters, in parentheses.

Example:

\begin{verbatim}
class
    on (dst 209.10.41.242)
    on (dst 199.183.24.194);
\end{verbatim}

This is actually a shortcut notation of

\raw{on element \meta{params}}

Also, the filter can be specified along with the element.

Example:

\begin{verbatim}
class
    on tcindex(mask 0) element (0);
\end{verbatim}

is equivalent to

\begin{verbatim}
tcindex(mask 0) {
    class
        on element(0);
}
\end{verbatim}

The number of a filter element can be explicitly set by including it
among the parameters. (Note: most filter elements require numbers to
be set this way. The only exception is \name{route}, which does not use
element numbers at all.)

Example:

\begin{verbatim}
    on (0x2e);
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Limitations}

It is currently not possible to combine \name{if} and \name{on}. Also,
all \name{if} constructs of a queuing discipline and its classes are
treated as part of a single filter.

Also, \name{on} can currently only be used to select classes, including class
selection paths. When using \name{drop}, only \name{if} is available.


%------------------------------------------------------------------------------


\section{Redefining fields}

Fields can be redefined, e.g. one may simplify the header structure by
assuming that packets contain no IP options (e.g. if such packets have
been eliminated before reaching traffic control) as follows:

\begin{verbatim}
#include "fields.tc"

field ip_nexthdr = ip[20];
\end{verbatim}

Note that, when redefining a field, only the fields that were known at
the time of the original definition, are available. It is therefore not
possible to construct recursive field definitions.

Fields definitions and redefinitions can appear anywhere where variable
definitions are allowed. They follow the same scoping rules as variables,
e.g. in

\begin{verbatim}
field port = tcp_sport;
prio {
    field port = tcp_dport;
    class if port == 80;
}
\end{verbatim}

\name{port} is the TCP destination port in the \name{prio} queuing
discipline, but becomes the source port again afterwards.


%------------------------------------------------------------------------------


\section{Tunnels}

The RSVP filter supports tunnels. They are specified by the keyword
\name{tunnel}, followed by the parameter list (in parentheses), a list of
selectors, and the tunnel body. All selectors inside the tunnel body
use the corresponding tunnel ID.

Example:

\begin{verbatim}
prio {
    class ();
    rsvp {
        tunnel (skip 2B)
            on (ah 17, dst 10.0.0.1)
        {
            class
                on (ipproto "tcp", dst 10.0.0.3);
        }
    }
}
\end{verbatim}

Note: the selector must not include an explicit filter reference.
Also, if an explicit filter reference is used on a class inside a
tunnel body, the tunnel has no effect on that class.


%------------------------------------------------------------------------------


\section{Policing}

If a selector matches a given packet, a rate-based policing decision can
be made in addition to this. The construct that describes an average
rate limit or a double leaky bucket, begins with the keyword \name{police},
followed by parameters. Optionally one or both of the following items
can follow: the action to take if the packet is found to exceed the
constraints, and the keyword \name{else} and the action to take in the
opposite case.

The following actions are possible:
\begin{description}
  \item[\name{pass}] accept the packet (default in non-exceed case)
  \item[\name{reclassify}] put the packet in an inferior class
    (default in exceed case)
  \item[\name{drop}] drop the packet
  \item[\name{continue}] ignore this match and continue with the next selector
\end{description}

Examples:

\begin{verbatim}
  on (0x2e)
    police (rate 100kbps, burst 2kB, mtu 1.5kB);
\end{verbatim}

\begin{verbatim}
  on (0x2e)
    police (rate 100kbps, burst 2kB, mtu 1.5kB) drop else continue;
\end{verbatim}


%------------------------------------------------------------------------------


\section{Old-style meters assignment}

Before compound expressions (section \ref{advcomp})
were added to the \prog{tcng} language,
meters had to be assigned using ``call by reference''. This was done
by passing the variable name as the first argument of the macros
that generate the meters. These macros have the same names as the
macros described in section \ref{meterstc}, but with the prefix
\raw{set\_}.

Example:

\begin{verbatim}
$meter = SLB(cir 50kbps,cbs 20kB);
set_SLB($meter,cir 50kbps,cbs 20kB);
\end{verbatim}

The two assignments are equivalent.

The \name{set\_\meta{meter}} macros are still available, but should
not be used when writing new \prog{tcng} configurations.


%==============================================================================
