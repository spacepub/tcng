%
% tcsim.inc - TC simulator
%
% Written 2001-2004 by Werner Almesberger
% Copyright 2001 EPFL-ICA
% Copyright 2002 Bivio Networks, Network Robots, Werner Almesberger
% Copyright 2003,2004 Werner Almesberger
%


\prog{tcsim} allows event-driven simulation of the behaviour of traffic
control
elements in user space. In order to simulate exactly what happens in the
kernel, \prog{tcsim} uses the original kernel source. It also uses the
\prog{tc} command-line
utility for configuration, so any consistency checks of \prog{tc}, and any
other effects caused by it (e.g. rounding of parameter values), are carefully
preserved too.

This is a reference manual for \prog{tcsim}, intended for readers
who are familiar with Linux traffic control. At several places, example
scripts like \path{examples/prio+rsvp} are mentioned. These scripts are
part of the \prog{tcng} package, and can be found in the directory
\path{tcng/examples}.

The examples assume that \prog{tcsim} is installed system-wide, and that
the commands are run from the \prog{tcng} top-level directory.
If running examples from the \prog{tcsim} directory, without installing
\prog{tcsim} first, command names need to be prefixed with \raw{./}, and
the path to example scripts needs to be prefixed with \raw{../}.

E.g. the example

\begin{verbatim}
tcsim examples/dsmark+policing | tcsim_filter | \
  tcsim_plot -t cumul
\end{verbatim}

becomes

\begin{verbatim}
./tcsim ../examples/dsmark+policing | ./tcsim_filter | \
  ./tcsim_plot -t cumul
\end{verbatim}

The \prog{tcng} package, and related material, can be found at
\url{http://tcng.sourceforge.net/}


%------------------------------------------------------------------------------


\section{Invocation}

\prog{tcsim} has a rather large number of command-line options. The most
commonly used ones are \raw{-v} and \raw{-s}.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Usage}
\label{tcsimusg}

\raw{tcsim} $[$\raw{-c}$]$ $[$\raw{-d} $[$\raw{-d}$]]$ $[$\raw{-g}$]$
  $[$\raw{-j}$]$ $[$\raw{-k} \meta{threshold}$]$ $[$\raw{-n}$]$
  $[$\raw{-p}$]$ $[$\raw{-q}$]$ $[$\raw{-s} \meta{snap\_len}$]$
  $[$\raw{-v} $\ldots]$ $[$\raw{-X\meta{phase},\meta{arg}}$]$
  $[$\meta{cpp\_option} $\ldots]$ $[$\meta{file}$]$

\raw{tcsim} \raw{-V}

\begin{description}
  \item[\raw{-c}] only check syntax, don't execute commands
  \item[\raw{-d}] print all kernel messages (\name{printk}). By default,
    \prog{tcsim} only prints messages with severity \name{KERN\_INFO} or
    higher.
  \item[\raw{-d -d}] also print \prog{tcsim} debugging messages
  \item[\raw{-g}] print generation numbers instead of skb addresses. This is
    mainly useful in regression tests, where output is compared with the
    output of previous runs.
  \item[\raw{-j}] print time in jiffies, the kernel's internal unit of time,
    not seconds
  \item[\raw{-k} \meta{threshold}] set the kernel logging threshold to the
    specified number. \raw{-k} overrides \raw{-d}.
  \item[\raw{-n}] do not include \name{default.tcsim}. By default, \prog{tcsim}
    includes this file, which in turn includes the files described
    in section \ref{tcsiminc}. This can be undesirable, e.g. if operating in
    a non-TCP/IP context, or if using a different default include file with
    application-specific definitions. In the latter case, the following
    options should be used: \\
    \verb"tcsim" $\ldots$ \verb"-n" $\ldots$ \verb"-XP,--include"
      \verb"-XP,/"\meta{directory}\verb"/"\meta{file} $\ldots$
  \item[\raw{-p}] preserve attributes across links (see section \ref{multi})
  \item[\raw{-q}] quiet operation. \prog{tcsim} does not generate output for
    \name{E} or \name{D} events, or \name{echo} commands. See sections
    \ref{debug} and \ref{trace}.
  \item[\raw{-s} \meta{snap\_len}] limit packet content dumped in trace
    output to \meta{snap\_len} bytes. See also section \ref{trace}.
  \item[\raw{-v}] enable function result tracing
  \item[\raw{-v -v}] also trace function invocation
  \item[\raw{-v -v -v}] also trace internal element state
  \item[\raw{-V}] print the version numbers of \prog{tcsim} and of the kernel
     and \name{iproute2} embedded in it, and exit
  \item[\raw{-X\meta{phase},\meta{argument}}] verbatim argument for specific
     build phase. See section \ref{phtcsim} for details.
  \item[\meta{cpp\_option}] one of the following options for the C
    pre-processor: \raw{-I\meta{dir}},
    \raw{-D\meta{name}}$[$\raw{=\meta{value}}$]$, or \raw{-U\meta{name}}
\end{description}

Examples:

A normal simulation run (we enqueue three packets in the order A, B, C,
and they are dequeued in the order A, C, B):

\begin{verbatim}
tcsim -s 20 examples/prio+rsvp
\end{verbatim}

The same simulation, with full tracing enabled:

\begin{verbatim}
tcsim -s 20 -v -v examples/prio+rsvp
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Phases underneath \prog{tcsim}}
\label{phtcsim}

Like \prog{tcng}, \prog{tcsim} can pass command-line options to programs
it invokes. 

Each such program invocation is called a ``phase'', and has a one-letter
name assigned to it. Options are passed to that specific phase, with the
command-line option \raw{-X\meta{phase},\meta{option}}

The phases and their names are shown in figure \ref{phtcsfig}.

\begin{figure}[ht]
\begin{center}
\epsfig{file=phtcsim.eps}
\end{center}
\caption{Parameter passing to phases underneath \prog{tcsim}.}
\label{phtcsfig}
\end{figure}

\prog{tcsim} also recognizes all phase names supported by \prog{tcng}
(see section \ref{phtcc}), and passes the command-line options to
\prog{tcng} for further distribution.

Note the \prog{tcsim}'s \prog{cpp} phase is identified by an upper case
``P'', while \prog{tcng}'s \prog{cpp} is identified by a lower case ``p''.


%------------------------------------------------------------------------------


\section{Configuration}

Simulations are described in a file. Each description begins with the
definition of interfaces, followed by the configuration of
traffic control elements (e.g. with a \prog{tcng} description or
a set of \prog{tc} commands), and then continues with instructions
for timing and packet enqueuing or dequeuing.

When using include files, they are usually included at the beginning
of the configuration file. Variable assignments may appear anywhere
in the file, and are typically placed after the configuration part.

Example:

\begin{verbatim}
#include "packet.def"
#include "ports.tc"

/* interface eth0, running at 10 Mbps */
dev eth0 10 Mbps {
    /* configuration in tcng:
       FIFO queue limited to 100 packets */
    fifo (limit 100p);
}

/* a few global variables */
$tcp_dport=1234
$tcp_sport=PORT_TELNET

/* send packet at time t=0s */
send TCP_PCK($tcp_SYN=1)

/* wait until t=1s, then send the next packet */
time 1s
send TCP_PCK($tcp_PSH=1)

/* wait until t=2s, then terminate */
time 2s
end
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{System definition}

\begin{description}
  \item[Syntax:] \raw{dev} \meta{name} $[$\meta{speed}$]$
    $[$\verb"{" \meta{tcng-spec} \verb"}"$]$
  \item[Example:] \verb"dev eth0 10 Mbps"
  \item[Effect:]
    Creates network interface \meta{name} sending at the specified
    rate. The rate is given as an integer followed by the unit:
    \name{bps} for bits per second, or \name{Bps} for bytes per
    second; plus optionally one of the prefixes \name{k} (kilo),
    \name{M} (mega), or \name{G} (giga). For backward-compatibility,
    the unit can be omitted. In this case, the unit defaults
    to kilobits per second.

    If the rate is omitted, the interface will
    only send when polled. The interface definition can be
    followed by a configuration specification in \prog{tcng} syntax,
    in curly braces. In this case, \prog{tcsim} invokes \prog{tcng} to
    translate the specification.

    The interface name begins with an alphabetic character or
    an underscore and is followed by any number of alphanumeric
    characters, or underscores. The interface name must not be a
    keyword of the \prog{tcsim} language (e.g. \name{send}, \name{dev},
    \name{x}, etc.)

    Alternatively, the interface name can be put in double quotes.
    In this case, any printable characters and also spaces are allowed
    in the name.
\end{description}

\begin{description}
  \item[Syntax:] \raw{host} \verb"{" \meta{items} $\ldots$ \verb"}"
  \item[Effect:]
    Defines a host structure. See section \ref{multi}, below.
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{System configuration}
\label{sysconf}

\begin{description}
  \item[Syntax:] \raw{insmod} \meta{path}
  \item[Example:] \verb"insmod modules/sch_discard.o"
  \item[Effect:]
    Loads the kernel part of an element built as an external object module.

    Note: such modules must have been compiled with \path{modules/kmod\_cc}
    Normal kernel modules will not work.
\end{description}

\begin{description}
  \item[Syntax:] \raw{preload} \meta{path}
  \item[Example:] \verb"preload modules/q_discard.so"
  \item[Effect:]
    Preloads the \name{iproute2/tc} part of an element built as an external
    object module.

    Note: such modules must have been compiled with \path{modules/tcmod\_cc}
\end{description}

\begin{description}
  \item[Syntax:] \raw{tc} \meta{argument} $\ldots$
  \item[Example:] \verb"tc add qdisc dev eth0 root prio"
  \item[Effect:]
    Issues the corresponding \prog{tc} command.
\end{description}

\begin{description}
  \item[Syntax:] \raw{attribute} 
    $[$\raw{default}$]$\meta{attribute}\raw{=}\meta{value} $\ldots$
  \item[Example:] \verb"attribute default protocol=ETH_P_IPV6"
  \item[Effect:]
    Changes the values and priorities of global default attributes.
    Attributes are described in more detail in section \ref{pckenq}.
    Note that there is only a single set of global defaults attributes,
    which is shared by all hosts configured in the simulator.
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Timing}

\begin{description}
  \item[Syntax:] \raw{time} \meta{time}
  \item[Example:] \verb"time 17.5ms"
  \item[Effect:]
    Advances the simulation clock to the specified absolute time. Any
    timers expiring before that time are triggered. It is an
    error to try to make time go backward.

    The time can be specified in seconds (e.g. \verb"5s") or in jiffies
    (e.g. \verb"0.2j"). Seconds and jiffies can be prefixed with \name{m}
    (milli), \name{k} (kilo), or \name{M} (mega).
    The time resolution is micro-seconds or micro-jiffies.

    The time can be given relative to the current time by prefixing it with a
    plus sign, e.g. \verb"time +5s"
\end{description}

\begin{description}
  \item[Syntax:] \raw{every} \meta{time} $[$\raw{until} \meta{time}$]$
    \meta{command}
  \item[Example:] \verb"every 100s send 0x41 0x00 0x00 0x14"
  \item[Effect:]
    Executes the specified command immediately, and then repeatedly after
    the specified interval. Only the commands \name{tc}, \name{send},
    \name{poll}, \name{every}, and \name{echo} can be used with
    \name{every}.

    If an end time is given, the \name{every} command is not longer
    executed after this (absolute) time. It is valid to specify an end
    time before the current time, in which case the \name{every}
    command has no effect. Like as with \name{time}, the end time can be
    specified relative to the current time by prefixing it with a plus sign.
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Termination}

\prog{tcsim} terminates in one of the following three ways:

\begin{itemize}
  \item ``clean'' termination with \name{end}
  \item immediate termination when reaching the end of the configuration
    file
 \item when encountering an error
\end{itemize}

The latter case deserves special attention because \prog{tcsim}
reads and processes commands as it proceeds with the simulation.
It may therefore detect an error in a configuration only after
possibly having spent a long while processing earlier parts of
the simulation. Long-running simulations should therefore be
checked for syntax errors with the \raw{-c} option first.

\begin{description}
  \item[Syntax:] \raw{end}
  \item[Example:] \verb"end"
  \item[Effect:]
    Stops all \name{every} commands, and waits for all timers to
    complete and all queues to empty.\footnote{\name{end} does this by
    simply advancing the simulation time to ``infinity'', a value larger
    than any time that can be specified in the simulation, and serving
    all events that occur until then, including new events generated by
    them.}

    All commands but \name{every} can follow \name{end}, although
    the use of \name{time} is discouraged. Typically, only \name{tc}
    commands are useful after \name{end}.
\end{description}

If a simulation does not use \name{end}, it simply stops when reaching
the end of the script, without waiting for timers.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Packet enqueuing}
\label{pckenq}

\begin{description}
  \item[Syntax:] \raw{send} $[$\meta{device}$]$
    $[[$\raw{default}$]$\meta{attribute}\raw{=}\meta{value} $\ldots]$
    \meta{value} $\ldots$
  \item[Example:] \verb"send 0x04 0x00 ns: 258 0x03"
  \item[Effect:]
    Enqueues the specified byte sequence. \prog{tcsim} assumes all
    packets to be IPv4, without link-layer header. \prog{tcsim}
    automatically sets the length field if the byte sequence appears indeed
    to be an IPv4 packet.\footnote{The heuristic used for this is that the
    first byte in the packet must be in the range 0x45$\ldots$0x4f, and the
    packet must contain at least twenty bytes.}

    The device name can be omitted if only a single device exists
    in the system when the \name{send} command is executed.

    The attributes set meta-data in the packet. The following
    attributes are supported:

   \begin{tabular}{lll}
     Attribute & Default & Description \\
     \hline
     \raw{nfmark}    & 0 & the network filter mark value \\
		     &   &  (used by the \name{fw} classifier) \\
     \raw{priority}  & 0 & priority value the ``real'' kernel derives from \\
		     &	 & the TOS byte or some socket options \\
     \raw{protocol}  & ETH\_P\_IP & protocol number obtained from link layer \\
     \raw{tc\_index} & 0 & shared traffic control decision \\
  \end{tabular}

  Attributes can be set with two priorities: ``default'', which is indicated
  by the keyword \raw{default} before the assignment, and ``normal'', which
  is indicated by the absence of that keyword. Setting an attribute with
  ``default'' priority doesn't change that attribute if it has previously
  been set with ``normal'' priority. In all other cases, the attribute is
  changed. Setting attributes with ``default'' priority is mainly used in
  macros, e.g. in \name{IP\_PCK} and \name{IP6\_PCK}.

  All attributes have a global default value and the default priority
  ``default''. This can be changed with the \name{attribute} command
  described in section \ref{sysconf}.

  Note: the attributes \raw{nfmark}, \raw{priority}, and \raw{tc\_index}
are usually reset to their global default values when crossing links.%
\footnote{\raw{protocol} is preserved because this information is typically
stored in link-layer headers, and is therefore preserved in real life too.}
  They are preserved when invoking \prog{tcsim}
  with the \raw{-p} option. (Use with caution: it's very easy to forget
  setting e.g. \verb"skb->tc_index", and things will still appear to
  work, because the value set at a previous host is re-used, which
  would of course not happen in real life.)

  Values are arithmetic or bit expressions, IPv4 addresses in
  dotted quad notation (e.g. \raw{10.0.0.1}), IPv6 addresses
  in any of the formats described in \cite{RFC2373}
  or variables (\raw{\$var}).
  Numeric constants can be given as decimal, hexadecimal, or
  binary (\raw{0b}$\ldots$) numbers. Also a Perl-like repetition
  operator \name{x} is available.

  By default, values are treated as bytes
  (Exception: dotted quads are treated as
  four bytes in network byte order.) A value can optionally be
  prefixed by a type:

  \begin{tabular}{ll}
    Prefix & Type \\
    \hline
    \raw{b:} & byte (8 bits) \\
    \raw{ns:} & network short (16 bits) \\
    \raw{hs:} & host short (16 bits) \\
    \raw{nl:} & network long (32 bits) \\
    \raw{hl:} & host long (32 bits) \\
    \raw{ipv4:} & IPv4 address (32 bits) \\
    \raw{ipv6:} & IPv6 address (128 bits) \\
  \end{tabular}

  \prog{tcsim} reports an error if a value is too large for the
  specified size.

  Example: \verb"ns: 258" is equivalent to \verb"0x01 0x02".

  Note that, when using a variable containing a value larger
  than a byte, the type needs to be specified.
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Packet dequeuing}

\prog{tcsim} can dequeue (``send'') packets either automatically, based on
the interface speed, or when explicitly requested with the \name{poll}
command. If no rate is specified on an interface, it will only send
packets if polled.

\begin{description}
  \item[Syntax:]  \raw{poll} $[$\meta{device}$]$
  \item[Example:] \verb"poll"
  \item[Effect:]
    Tries to dequeue packets. If no packets are available, \name{poll}
    has no effect. If no device name is specified, \name{poll} polls all
    available devices.
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Debugging}
\label{debug}

\begin{description}
  \item[Syntax:]  \raw{echo} \meta{expression}$|$\meta{quoted\_string} $\ldots$
  \item[Example:] \verb|echo 1 "<<" 8 "is" 0x%x 1 << 8 "(hex)"|
  \item[Effect:]
    Prints the arguments, separated by spaces. An argument is
    either a string in double quotes (e.g. \verb|"string"|), or an
    expression. Expressions can be prefixed with a printf-style
    format string. The following strings are currently recognized:

  \begin{tabular}{ll}
    \verb"%u"   & unsigned decimal integer, e.g. 100 is printed as 100 \\
    \verb"%x"   & unsigned hexadecimal integer, e.g. 100 is printed as 64 \\
    \verb"0x%x" & like \verb"%x", but add \verb"0x" prefix,
	          e.g. 100 is printed as 0x64 \\
  \end{tabular}

    \name{echo} commands have no effect if using the \raw{-q} option.
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Configuration file format}

The \name{tc}, \name{insmod}, and \name{preload} commands terminate at the
end of the line. Lines can be continued by ending them with a backslash.
All other commands terminate at the next command keyword.

C-style comments can be used anywhere in a \prog{tcsim} configuration
file. Outside of \prog{tcng} configuration sections, comments can also
be started with a semicolon (\verb";"). Such comments extend to the end
of the line. 

Example:

\begin{verbatim}
/* This is a comment */
dev eth0 1Mbps; this is also a comment
{
    prio; // comment: semicolons in tcng configuration
          // sections are NOT interpreted as comment
          // characters
}
\end{verbatim}

The description file may contain \prog{cpp} directives. Note that
also \prog{cpp} directives in \prog{tcng} configuration sections
are processed by \prog{tcsim}. Therefore, the following example
would not work as probably intended:

\begin{verbatim}
#define N 5

dev eth0 {
    #define N 10
    fifo (limit N p);
}

send 0 x N
\end{verbatim}


%------------------------------------------------------------------------------


\section{Variables}

The configuration language also provides simple variable handling
capabilities. Variables are mainly used for constructing packet commands,
typically in \prog{cpp} macros.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Numeric variables}

Variable assignments are of the form
\raw{\$\meta{var}=\meta{expression}}. Variable assignments can appear
either before a command, or between values in the \name{send} command.

Variable references are of the form \raw{\$\meta{var}}, and can appear
anywhere where an integer number or a dotted quad could be used, except in a
\name{tc}, \name{insmod}, or \name{preload} command.

Example:

\begin{verbatim}
#define foo(params) \
  $answer=42 params \
  $answer $value

send foo($value=5)
send foo($value=10 $answer=8)
\end{verbatim}

yields the following two packets:

\begin{verbatim}
2a05
080a
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Common idioms}
\label{idioms}

The example above shows an idiom frequently used in packet construction
scripts: a variable is set to a default value in a macro, and can then
be superseded by assignments passed as a macro parameter.

Similarly, variables without default value, or with a global default,
can be set using the same syntax.

While this idiom makes effective use of the rather limited variable
handling capabilities of \prog{tcsim}, and is syntactically elegant,
it is not without dangers: because \prog{tcsim} variables are always
global, variables that are not explicitly reset to a default value
will retain their value also after the macro in which they are set.

Example:

\begin{verbatim}
#define foo(params) \
  $value=0 /* local default */ \
  params $value

send foo($value=8)
send foo()
\end{verbatim}

yields

\begin{verbatim}
08
00
\end{verbatim}

but

\begin{verbatim}
$value=0 /* global default */

#define foo(params) \
  params $value

send foo($value=8)
send foo()
\end{verbatim}

yields

\begin{verbatim}
08
08
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Command variables}

In order to process scripts with many \name{send} command efficiently (e.g.
in a trace-driven simulation), the command can be stored in a variable.
Command variables are set with the syntax:

\raw{command \$\meta{var} = \meta{command}}

They are referenced with \raw{command \$\meta{var}}, e.g.

\begin{verbatim}
time 1s command $var
\end{verbatim}

All command that can be used with \name{every} can be stored in a variable.


%------------------------------------------------------------------------------


\section{Using multiple interfaces}
\label{multi}

\prog{tcsim} can also simulate configurations involving multiple
hosts. This is mainly useful for examining the interaction of
several stages of traffic conditioning, e.g. how a shaped flow
is treated by an ingress policer.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Configuration}

When using multiple interfaces, the usage of \prog{tcsim} changes as follows:
\begin{itemize}
  \item the output interface has to be specified in the \name{send} command
  \item the \name{poll} command takes an interface name as an optional
    argument
\end{itemize}

Furthermore, the \name{host} construct is used to define logical groupings
of devices and to specify routes:

\begin{verbatim}
host {
    dev ...
    tc ...
    route ...
    ...
}
\end{verbatim}

The \name{dev} and \name{tc} commands are used like in single-interface
configurations. \name{route} is used as follows:

\begin{description}
  \item[Syntax:]
     \raw{route} \meta{destination} $[$\raw{netmask} \meta{netmask}$]$
       \meta{device} \\
     \raw{route default} \meta{device}
  \item[Example:] \verb"route 10.0.0.0 netmask 255.0.0.0 b_eth1"
\end{description}

It is an error to try to enter a more specific route after entering
a more general route. (This restriction may be removed in the future.)

Interfaces are connected with the \name{connect} command:

\begin{description}
  \item[Syntax:] \raw{connect} \meta{device} \meta{device}
  \item[Example:] \verb"connect a_eth0 b_eth0"
\end{description}

\name{connect} may appear inside a \name{host} construct or at any
other place where commands are allowed. \name{connect} creates a
bidirectional connection. Connections in \prog{tcsim} currently
have no delay.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Example}

The configuration example \path{examples/ingress} contains the following
setup:

\begin{verbatim}
dev a_eth0 10 Mbps

host {
    dev b_eth0 10 Mbps
    dev b_eth1 10 Mbps
    route 10.0.0.0 netmask 255.0.0.0 b_eth1
}

connect a_eth0 b_eth0

send a_eth0 PACKET
\end{verbatim}

Since \prog{tcsim} does not name hosts, we include the host name
in interface names for clarity. The interface \name{a\_eth0}
does not require a host section, because packets are enqueued
directly at that interface, so no other ``local'' interface needs
to be associated with it.

Figure \ref{fmulti} illustrates the configuration generated by the
above example.

\begin{figure}[ht]
\begin{center}
\epsfig{file=multi.eps}
\end{center}
\caption{Configuration with multiple interfaces.}
\label{fmulti}
\end{figure}


%------------------------------------------------------------------------------


\section{Include files}
\label{tcsiminc}

\prog{tcsim} comes with a set of include files that provide building
blocks for configurations. For convenience, \name{packet.def} and
\name{ports.tc} (from \prog{tcng}, see section \ref{portstc}) are automatically
included by default. Section \ref{tcsimusg} describes how this can be
disabled.

The file \name{packet.def} contains macros for constructing IPv4
packets, with various transport layer protocols (TCP, UDP, ICMP, etc.).
The actual content of \name{packet.def} is in the files \name{packet4.def}
and \name{packet6.def}, which are included by \name{packet.def}, and
should not be included individually.

\name{ip.def} is the predecessor of \name{packet.def}. While still
extensively used in regression tests and examples, \name{ip.def}
is obsolete and should be avoided in new configuration scripts.

The file \name{tcngreg.def} is only used internally by the \prog{tcng}
package for regression tests.

\prog{tcsim} include files are normally installed in the directory
\url{/usr/lib/tcng/include/}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Packet construction}
\label{packetdef}

\name{packet.def} (actually \name{packet4.def} and \name{packet6.def},
see above) defines headers for IPv4, IPv6, TCP, UDP, ICMP, and IGMP,
and it also provides macros for constructing entire packets.

The following sections describe the header structures, the variables
used to populate header fields, and their default values. Values which
are only set globally, and which retain their value after each change
(see section \ref{idioms}) are marked with \emphasize{global}.

The variable names in \name{packet.def} are the same used in
\name{fields.tc}. Section \ref{fieldstc} describes \name{fields.tc}
in more detail.

Note that \name{packet.def} includes \name{values.tc}, described
in section \ref{valuestc}.


% -----


\subsubsection{IPv4 header}

The macro \name{IP\_HDR} builds an IPv4 header, as defined in
\cite{RFC791}. The macro \name{IP\_PCK} is synonymous to
\name{IP\_HDR}, except that it also sets the \name{protocol} to
\name{ETH\_P\_IP} with ``default'' priority.
$$
\begin{tabular}{|cccccccccccccccccccccccccccccccc|}
  \multicolumn{2}{c}{0} &
  \multicolumn{2}{c}{2} &
  \multicolumn{2}{c}{4} &
  \multicolumn{2}{c}{6} &
  \multicolumn{2}{c}{8} &
  \multicolumn{2}{c}{10} &
  \multicolumn{2}{c}{12} &
  \multicolumn{2}{c}{14} &
  \multicolumn{2}{c}{16} &
  \multicolumn{2}{c}{18} &
  \multicolumn{2}{c}{20} &
  \multicolumn{2}{c}{22} &
  \multicolumn{2}{c}{24} &
  \multicolumn{2}{c}{26} &
  \multicolumn{2}{c}{28} &
    \multicolumn{2}{c}{30} \\
  \hline
  \multicolumn{4}{|c|}{\raw{\$ip\_v}} &
    \multicolumn{4}{c|}{\raw{\$ip\_hl}} &
    \multicolumn{8}{c|}{\raw{\$ip\_tos}} &
    \multicolumn{16}{c|}{Total length} \\
  \multicolumn{4}{|c|}{4} &
    \multicolumn{4}{c|}{5} &
    \multicolumn{8}{c|}{0} &
    \multicolumn{16}{c|}{set by \prog{tcsim}} \\
  \hline
  \multicolumn{16}{|c|}{\raw{\$ip\_id}} &
    \multicolumn{3}{c|}{Flags} &
    \multicolumn{13}{c|}{\raw{\$ip\_off}} \\
  \multicolumn{16}{|c|}{0} &
    \multicolumn{3}{c|}{see below} &
    \multicolumn{13}{c|}{0} \\
  \hline
  \multicolumn{8}{|c|}{\raw{\$ip\_ttl}} &
    \multicolumn{8}{c|}{\raw{\$ip\_proto}} &
    \multicolumn{16}{c|}{\raw{\$ip\_sum}} \\
  \multicolumn{8}{|c|}{64} &
    \multicolumn{8}{c|}{\emphasize{global} 0} &
    \multicolumn{16}{c|}{0} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$ip\_src}} \\
  \multicolumn{32}{|c|}{\emphasize{global} 10.0.0.1} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$ip\_dst}} \\
  \multicolumn{32}{|c|}{\emphasize{global} 10.0.0.2} \\
  \hline
\end{tabular}
$$

Note that \raw{\$ip\_src} and \raw{\$ip\_dst} are of type ``IPv4
address''.

The following flags are used for handling IP fragments:
$$
\begin{tabular}{|c|c|c|}
  \multicolumn{1}{c}{0} &
    \multicolumn{1}{c}{1} &
    \multicolumn{1}{c}{2} \\
  \hline
  \raw{\$ip\_RF} & \raw{\$ip\_DF} & \raw{\$ip\_MF} \\
  0 & 0 & 0 \\
  \hline
\end{tabular}
$$

Example:

\begin{verbatim}
$ip_dst = 1.2.3.4

send IP_PCK($ip_tos=0xe0 $ip_proto=46)
  /* pretend this is RSVP */
\end{verbatim}


% -----


\subsubsection{TCP header}

The macro \name{TCP\_HDR} builds a TCP header, as defined in
\cite{RFC793}. The macro \name{TCP\_PCK} prepends an IPv4
header to the TCP header.
$$
\begin{tabular}{|cccccccccccccccccccccccccccccccc|}
  \multicolumn{2}{c}{0} &
  \multicolumn{2}{c}{2} &
  \multicolumn{2}{c}{4} &
  \multicolumn{2}{c}{6} &
  \multicolumn{2}{c}{8} &
  \multicolumn{2}{c}{10} &
  \multicolumn{2}{c}{12} &
  \multicolumn{2}{c}{14} &
  \multicolumn{2}{c}{16} &
  \multicolumn{2}{c}{18} &
  \multicolumn{2}{c}{20} &
  \multicolumn{2}{c}{22} &
  \multicolumn{2}{c}{24} &
  \multicolumn{2}{c}{26} &
  \multicolumn{2}{c}{28} &
    \multicolumn{2}{c}{30} \\
  \hline
  \multicolumn{16}{|c|}{\raw{\$tcp\_sport}} &
    \multicolumn{16}{c|}{\raw{\$tcp\_dport}} \\
  \multicolumn{16}{|c|}{\emphasize{global} 0} &
    \multicolumn{16}{c|}{\emphasize{global} 0} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$tcp\_seq}} \\
  \multicolumn{32}{|c|}{0} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$tcp\_ack}} \\
  \multicolumn{32}{|c|}{0} \\
  \hline
  \multicolumn{4}{|c|}{\raw{\$tcp\_off}} &
    \multicolumn{6}{c|}{Reserved} &
    \multicolumn{6}{c|}{\raw{\$tcp\_flags}} &
    \multicolumn{16}{c|}{\raw{\$tcp\_win}} \\
  \multicolumn{4}{|c|}{5} &
    \multicolumn{6}{c|}{0} &
    \multicolumn{6}{c|}{see below} &
    \multicolumn{16}{c|}{0} \\
  \hline
  \multicolumn{16}{|c|}{\raw{\$tcp\_sum}} &
    \multicolumn{16}{c|}{\raw{\$tcp\_urp}} \\
  \multicolumn{16}{|c|}{0} &
    \multicolumn{16}{c|}{0} \\
  \hline
\end{tabular}
$$

The TCP flags can either be set through \raw{\$tcp\_flags}, or through the
variables below. The values of \raw{\$tcp\_flags} and the single-bit
variables are simply or-ed, so setting \raw{\$tcp\_flags} and any of
the single-bit variables to a value other than zero may not yield the
desired result.
$$
\begin{tabular}{|c|c|c|c|c|c|}
  \multicolumn{1}{c}{0} &
    \multicolumn{1}{c}{1} &
    \multicolumn{1}{c}{2} &
    \multicolumn{1}{c}{3} &
    \multicolumn{1}{c}{4} &
    \multicolumn{1}{c}{5} \\
  \hline
  \raw{\$tcp\_URG} & \raw{\$tcp\_ACK} & \raw{\$tcp\_PSH} &
  \raw{\$tcp\_RST} & \raw{\$tcp\_SYN} & \raw{\$tcp\_FIN} \\
  0 & 0 & 0 &
  0 & 0 & 0 \\
  \hline
\end{tabular}
$$

Example:

\begin{verbatim}
$tcp_sport=3445 /* ephemeral port */
$tcp_dport=80   /* HTTP */

send IP_HDR($ip_proto=IPPROTO_TCP $ip_tos=0xb8)
     TCP_HDR($tcp_SYN=1)
send TCP_PCK($ip_tos=0xb8 $tcp_SYN=1) /* equivalent */
\end{verbatim}


% -----


\subsubsection{UDP header}

The macro \name{UDP\_HDR} builds a UDP header, as defined in
\cite{RFC768}. The macro \name{UDP\_PCK} prepends an IPv4
header to the UDP header.
$$
\begin{tabular}{|cccccccccccccccccccccccccccccccc|}
  \multicolumn{2}{c}{0} &
  \multicolumn{2}{c}{2} &
  \multicolumn{2}{c}{4} &
  \multicolumn{2}{c}{6} &
  \multicolumn{2}{c}{8} &
  \multicolumn{2}{c}{10} &
  \multicolumn{2}{c}{12} &
  \multicolumn{2}{c}{14} &
  \multicolumn{2}{c}{16} &
  \multicolumn{2}{c}{18} &
  \multicolumn{2}{c}{20} &
  \multicolumn{2}{c}{22} &
  \multicolumn{2}{c}{24} &
  \multicolumn{2}{c}{26} &
  \multicolumn{2}{c}{28} &
    \multicolumn{2}{c}{30} \\
  \hline
  \multicolumn{16}{|c|}{\raw{\$udp\_sport}} &
    \multicolumn{16}{c|}{\raw{\$udp\_dport}} \\
  \multicolumn{16}{|c|}{\emphasize{global} 0} &
    \multicolumn{16}{c|}{\emphasize{global} 0} \\
  \hline
  \multicolumn{16}{|c|}{\raw{\$udp\_ulen}} &
    \multicolumn{16}{c|}{\raw{\$udp\_sum}} \\
  \multicolumn{16}{|c|}{0} &
    \multicolumn{16}{c|}{0} \\
  \hline
\end{tabular}
$$

Example:

\begin{verbatim}
$udp_sport=53   /* DNS */

send UDP_PCK($ip_src=198.41.0.4 $udp_sport=5432)
\end{verbatim}


% -----


\subsubsection{ICMP header}

The macro \name{ICMP\_HDR} builds the first four bytes of an ICMP header,
as defined in \cite{RFC792}. For a valid ICMP message, at least for more
bytes must be appended. The format of the rest of the message depends on
the message type.

The macro \name{ICMP\_PCK} prepends an IPv4 header to the partial ICMP
header constructed with \name{ICMP\_HDR}.
$$
\begin{tabular}{|cccccccccccccccccccccccccccccccc|}
  \multicolumn{2}{c}{0} &
  \multicolumn{2}{c}{2} &
  \multicolumn{2}{c}{4} &
  \multicolumn{2}{c}{6} &
  \multicolumn{2}{c}{8} &
  \multicolumn{2}{c}{10} &
  \multicolumn{2}{c}{12} &
  \multicolumn{2}{c}{14} &
  \multicolumn{2}{c}{16} &
  \multicolumn{2}{c}{18} &
  \multicolumn{2}{c}{20} &
  \multicolumn{2}{c}{22} &
  \multicolumn{2}{c}{24} &
  \multicolumn{2}{c}{26} &
  \multicolumn{2}{c}{28} &
    \multicolumn{2}{c}{30} \\
  \hline
  \multicolumn{8}{|c|}{\raw{\$icmp\_type}} &
  \multicolumn{8}{c|}{\raw{\$icmp\_code}} &
    \multicolumn{16}{c|}{\raw{\$icmp\_sum}} \\
  \multicolumn{8}{|c|}{\emphasize{global} 8} &
  \multicolumn{8}{c|}{\emphasize{global} 0} &
    \multicolumn{16}{c|}{0} \\
  \hline
  \multicolumn{32}{|c|}{$\ldots$} \\
  \multicolumn{32}{|c|}{} \\
  \hline
\end{tabular}
$$

The default value for \raw{\$icmp\_type} (8) is the type code for an
ICMP Echo message.

Example:

\begin{verbatim}
$identifier = 42
$sequence_number = 2
send ICMP_PCK($ip_ttl=4)
  ns: $identifier ns: $sequence_number
\end{verbatim}


% -----


\subsubsection{IGMP header}

The macro \name{IGMP\_HDR} builds an IGMP header,
as defined in \cite{RFC2236}.
The macro \name{IGMP\_PCK} prepends an IPv4 header to the IGMP header.
$$
\begin{tabular}{|cccccccccccccccccccccccccccccccc|}
  \multicolumn{2}{c}{0} &
  \multicolumn{2}{c}{2} &
  \multicolumn{2}{c}{4} &
  \multicolumn{2}{c}{6} &
  \multicolumn{2}{c}{8} &
  \multicolumn{2}{c}{10} &
  \multicolumn{2}{c}{12} &
  \multicolumn{2}{c}{14} &
  \multicolumn{2}{c}{16} &
  \multicolumn{2}{c}{18} &
  \multicolumn{2}{c}{20} &
  \multicolumn{2}{c}{22} &
  \multicolumn{2}{c}{24} &
  \multicolumn{2}{c}{26} &
  \multicolumn{2}{c}{28} &
    \multicolumn{2}{c}{30} \\
  \hline
  \multicolumn{8}{|c|}{\raw{\$igmp\_type}} &
  \multicolumn{8}{c|}{\raw{\$igmp\_code}} &
    \multicolumn{16}{c|}{\raw{\$igmp\_sum}} \\
  \multicolumn{8}{|c|}{\emphasize{global} 0x11} &
  \multicolumn{8}{c|}{0} &
    \multicolumn{16}{c|}{0} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$igmp\_group}} \\
  \multicolumn{32}{|c|}{0} \\
  \hline
\end{tabular}
$$

The default value for \raw{\$igmp\_type} (0x11) is the type code for an
IGMP membership query message.

Note that \raw{\$igmp\_group} is of type ``IPv4 address''.

Example:

\begin{verbatim}
#define MC_ALL_ROUTERS 224.0.0.2

send IGMP_PCK($igmp_type=IGMP_V1_MEMBERSHIP_REPORT
  $igmp_group=MC_ALL_ROUTERS)
\end{verbatim}


% -----


\subsubsection{IPv6 header}

The macro \name{IP6\_HDR} builds an IPv6 header, as defined in
\cite{RFC2460}. The macro \name{IP6\_PCK} is synonymous to
\name{IP6\_HDR}, except that it also sets the \name{protocol} to
\name{ETH\_P\_IPV6} with ``default'' priority.
$$
\begin{tabular}{|cccccccccccccccccccccccccccccccc|}
  \multicolumn{2}{c}{0} &
  \multicolumn{2}{c}{2} &
  \multicolumn{2}{c}{4} &
  \multicolumn{2}{c}{6} &
  \multicolumn{2}{c}{8} &
  \multicolumn{2}{c}{10} &
  \multicolumn{2}{c}{12} &
  \multicolumn{2}{c}{14} &
  \multicolumn{2}{c}{16} &
  \multicolumn{2}{c}{18} &
  \multicolumn{2}{c}{20} &
  \multicolumn{2}{c}{22} &
  \multicolumn{2}{c}{24} &
  \multicolumn{2}{c}{26} &
  \multicolumn{2}{c}{28} &
    \multicolumn{2}{c}{30} \\
  \hline
  \multicolumn{4}{|c|}{\raw{\$ip6\_v}} &
    \multicolumn{8}{c|}{\raw{\$ip6\_tc}} &
    \multicolumn{20}{c|}{\raw{\$ip6\_flow}} \\
  \multicolumn{4}{|c|}{6} &
    \multicolumn{8}{c|}{0} &
    \multicolumn{20}{c|}{0} \\
  \hline
  \multicolumn{16}{|c|}{\raw{\$ip6\_plen}} &
    \multicolumn{8}{c|}{\raw{\$ip6\_nxt}} &
    \multicolumn{8}{c|}{\raw{\$ip6\_hlim}} \\
  \multicolumn{16}{|c|}{0} &
    \multicolumn{8}{c|}{\emphasize{global} 0} &
    \multicolumn{8}{c|}{64} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$ip6\_src}} \\
  \multicolumn{32}{|c|}{\emphasize{global} ::} \\
  \hline
  \multicolumn{32}{|c|}{\raw{\$ip6\_dst}} \\
  \multicolumn{32}{|c|}{\emphasize{global} ::} \\
  \hline
\end{tabular}
$$

Note that \raw{\$ip6\_src} and \raw{\$ip6\_dst} are of type ``IPv6
address''.

Example:

\begin{verbatim}
$ip6_dst = ::13.1.68.3

send IP6_PCK($ip6_tc=0xe0 $ip6_nxt=46)
  /* pretend this is RSVP */
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Related include files}

\name{values.tc} from \prog{tcng} contains definitions of various
well-known values used in TCP/IP headers. \name{values.tc} is
automatically included when using \name{packet.def}. It is
described in more detail in section \ref{valuestc}.

\name{ports.tc} from \prog{tcng} contains definitions for most
IANA-assigned port numbers. If is defined in section \ref{portstc}.


%------------------------------------------------------------------------------


\section{Simulation output}

By default, \prog{tcsim} prints a message whenever a packet is
enqueued or dequeued, or when some exceptional condition (e.g. an
error) occurs. This output can be post-processed to extract
statistical data, or to generate a graphical representation of
traffic characteristics.

\prog{tcsim} can also provide more detailed information on the inner
workings of the traffic control subsystem, which is useful for testing
configurations and the development of new traffic control elements.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Raw \prog{tcsim} output}
\label{trace}

\prog{tcsim} prints tracing information on standard output.
Each line has the following format:

\meta{time} \meta{event} \raw{:} $\ldots$

\begin{description}
  \item[\meta{time}] The simulation time at which the even has occurred,
    either in seconds or in jiffies.
  \item[\meta{event}] The event type. Currently, the following events
    are reported:

  \begin{tabular}{ll}
    \raw{I} & ingress queuing discipline \\
    \raw{E} & device enqueue \\
    \raw{D} & device dequeue \\
    \raw{T} & execution of a \name{tc} command \\
    \raw{*} &  error or diagnostic \\
  \end{tabular}

    Additionally, if function call tracing is enabled (with \raw{-v}),
    the following events are reported:

  \begin{tabular}{ll}
    \raw{c} & call to classifier \\
    \raw{i} & call to enqueue function of the ingress queuing discipline \\
    \raw{e} & call to enqueue function of an egress queuing discipline \\
    \raw{d} & call to dequeue function \\
    \raw{r} & call to requeue function \\
    \raw{x} & call to drop function \\
    \raw{s} & call to \name{reshape\_fail} callback \\
    \raw{p} & call to \name{tcf\_police} \\
  \end{tabular}
\end{description}

For device enqueue and dequeue events, the following additional information
is printed:

\meta{skb\_addr} \meta{length} \raw{:} \meta{device}\raw{:}
  \meta{content} $\ldots$

\begin{description}
  \item[\meta{skb\_addr}] The address of the skb.
  \item[\meta{length}] Length of the packet.
  \item[\meta{content}]	Packet content, printed in groups of four hex bytes.
    The length can be limited with the \raw{-s} option. If bytes were
    not displayed due to \raw{-s}, \prog{tcsim} ends the line with three
    dots (\verb"...")
\end{description}

For \prog{tc} command execution, the command line (including the \prog{tc}
command) is printed.

For all tracing events, the following information is printed:

\meta{skb\_addr} \meta{length} \raw{:} \raw{$<$}\meta{level}\raw{$>$}
  \meta{descr} $\ldots$

\begin{description}
  \item[\meta{skb\_addr}] Address of the skb, 0x0 if no skb is available
  \item[\meta{length}] Length of the packet, 0 if no skb is available
  \item[\meta{level}] Call depth (enclosed by \verb"<>"), starting at zero.
  \item[\meta{descr}] Description of the event, usually including the kind
    and ID of the element, and the return value of the function
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Pretty-printing traces}

The script \name{tcsim\_pretty} can be used to format traces in a more
human-readable way. E.g.

\begin{verbatim}
tcsim -s 20 -v -v -v examples/dsmark+policing | \
  tcsim_pretty
\end{verbatim}

\name{tcsim\_pretty} usually wraps long lines. The default line width of
79 characters can be changed with the \raw{-w \meta{width}} option.
Wrapping is turned off with the \raw{-l} option.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Output filtering}

Enqueue and dequeue records can be selected in trace output with the
\name{tcsim\_filter} script.

Usage:

\raw{tcsim\_filter} $[$\raw{-c}$]$ $[$\raw{-s \meta{time}} $\ldots]$
  $[$\raw{-e}$|$\raw{-d}$]$
  $[$\meta{selector} $\ldots]$ $[$\meta{output} $\ldots]$
  $[$\meta{file} $...]$

\begin{description}
  \item[\raw{-c}] Count the results instead of printing data points on
    standard output.
  \item[\raw{-s \meta{time}}] Write counter snapshot to standard output at the
    specified time (in seconds). This option can be repeated any number of
    times. If \name{tcsim\_filter} output is piped to \name{tcsim\_plot}
    (see section \ref{tcsimplot}),
    the latter will propagate snapshots to its standard output.
  \item[\raw{-e}] Only consider enqueue events (default: enqueue and dequeue)
  \item[\raw{-d}] Only consider dequeue events
  \item[\meta{selector}] Expression of the type
    \meta{field}\raw{=}\meta{value}. Only records where the
    field has the specified value will be shown or counted.
  \item[\meta{output}] Include the value of the specified field in the output.
  \item[\meta{file}] Read from the specified file. Default: read from standard
    input.
\end{description}

The following fields are recognized:

\begin{tabular}{ll}
  \name{tos} & TOS byte \\
  \name{len} & Total length field \\
  \name{src} & Source IP address \\
  \name{dst} & Destination IP address \\
  \name{sport} & Source port (TCP or UDP) \\
  \name{dport} & Destination port (TCP or UDP) \\
  \name{dev} & Device name (e.g. eth0) \\
\end{tabular}

Selected records are condensed into an identification string that begins
with the event type (\raw{E} or \raw{D}), followed by the hexadecimal
values of all output fields, separated by colons.

When counting, the records with the same ID string are counted. When
printing records, each line contains the time, the ID string, the
packet length in bytes, and the skb address (or, if using
\raw{-g}, the generation number), separated by spaces.

Examples:

\begin{verbatim}
tcsim examples/dsmark+policing | tcsim_filter -c tos
D:00 201
D:b8 139
E:00 201
E:01 201
\end{verbatim}

\begin{verbatim}
tcsim examples/dsmark+policing | tcsim_filter -c tos=0xb8
D 141
\end{verbatim}

\begin{verbatim}
tcsim examples/dsmark+policing | tcsim_filter src | sed 4q
0.000000 E:0a000001 1000 0x80b7888
0.000000 D:0a000001 1000 0x80b7888
0.000000 E:0a000001 1000 0x80ba060
0.000800 D:0a000001 1000 0x80ba060
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Graphical output}
\label{tcsimplot}

Filtered output can be further processed with the script \name{tcsim\_plot},
which uses \name{gnuplot} to generate plots.

Usage:

\raw{tcsim\_plot} $[$\raw{-a} \meta{samples}$]$ $[$\raw{-j}$]$
  $[$\raw{-m}$]$
  $[$\raw{-p} \meta{ps\_file}$|$\raw{-jpeg} \meta{jpeg\_file}$|$%
\raw{-png} \meta{png\_file}$]$
  $[$\raw{-t} \meta{plot\_type}$]$
  $[$\raw{-cmd} \meta{gnuplot\_command} $\ldots$$]$ $[$\meta{file} $\ldots]$

\begin{description}
  \item[\raw{-a} \meta{samples}] Average over the number of samples
  \item[\raw{-j}] Time is in jiffies, not seconds (i.e. \prog{tcsim} was run
    with \raw{-j})
  \item[\raw{-m}] Generate monochrome output
  \item[\raw{-cmd} \meta{gnuplot\_command}] Send command to \prog{gnuplot}
    before plotting
  \item[\raw{-p} \meta{ps\_file}] Generate Postscript output
  \item[\raw{-jpeg} \meta{jpeg\_file}] Generate JPEG output
  \item[\raw{-png} \meta{png\_file}] Generate PNG (Portable Network Graphics)
    output
  \item[\raw{-t} \meta{plot\_type}] Specifies the plot type
\end{description}

If the file name for \raw{-p}, \raw{-jpeg}, or \raw{-png} is \verb"-",
\name{tcsim\_plot} writes the data to standard output and suppresses
forwarding of \name{tcsim\_filter} snapshots.

The following plot types are available:
\begin{description}
  \item[\name{rate}] Bit rate (based on the inter-arrival time)
  \item[\name{iat}] Packet inter-arrival time
  \item[\name{cumul}] Cumulative amount of data
  \item[\name{delay}] Queuing delay, measured at dequeue time
\end{description}

\name{rate} and \name{iat} normally require averaging in order to produce
useful results. With \name{rate}, packets apparently arriving at the same
time are treated like one large packet arriving at that time.

Examples:

\begin{verbatim}
tcsim examples/dsmark+policing | tcsim_filter | \
  tcsim_plot -t cumul
\end{verbatim}

\begin{verbatim}
tcsim examples/dsmark+policing | tcsim_filter | \
  tcsim_plot -a 10
\end{verbatim}

\begin{verbatim}
tcsim examples/ef-prio | tcsim_filter tos | \
  tcsim_plot -t delay
\end{verbatim}


%------------------------------------------------------------------------------


\section{Adding new traffic control elements}

There are two ways for making new components available in \prog{tcsim}:
\begin{itemize}
  \item adding them directly to the \prog{tcsim} executable.
    This is how \prog{tcsim} obtains all components provided by the kernel.
  \item building them as external loadable modules
\end{itemize}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Adding elements to \prog{tcsim}}

New traffic control elements are added to the \prog{tcsim} executable as
follows:

\begin{itemize}
  \item first, add the necessary code to the kernel source tree and to
    \name{iproute2/tc}
  \item edit the file \name{config} or run \name{configure} so that
    \prog{tcsim} can find and use the new source trees
  \item in \name{setup.klib}, add a command to copy the new kernel source
    file(s) to the \path{klib} directory
  \item add the name(s) of the new kernel code object file(s) to \name{OBJS} in
    \name{Makefile.klib}
  \item if your component requires any new kernel configuration option(s)
    to be set, add them to \path{include/linux/config.h} in \path{setup.klib}
    (search for \raw{CONFIG})
  \item if your component requires a new entry in \path{iproute2/Config},
    enable it in \path{setup.ulib} (search for \raw{Config})
  \item run \name{make}
\end{itemize}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Including modules}

Modules are used as follows:

\begin{itemize}
  \item build ``kernel'' modules with 
\begin{verbatim}
kmod_cc -c -D__KERNEL__ -DMODULE -O -fomit-frame-pointer \
  -fno-strict-aliasing -I/usr/src/linux/include \
  -o module.o module.c
\end{verbatim}
 \item build \name{iproute2/tc} modules with
\begin{verbatim}
tcmod_cc -shared -o module.so module.c
\end{verbatim}
 \item in the simulation, load the ``kernel''' module with the \name{insmod}
   command. If the \name{iproute2/tc} module is in a directory where
   \name{dlopen(3)} can find it, it is automatically loaded. Otherwise, load
   it with the \name{preload} command.
\end{itemize}

The \name{kmod\_cc} command-line options are the same as used when building
real kernel modules.
\name{kmod\_cc} automatically adjusts the command line such that
\prog{tcsim}'s private includes are used.

Note: there may be include file problems when building for \prog{tcsim}, even
if your code normally compiles perfectly. The reason for this is that
\prog{tcsim} replaces many kernel include files in order to limit the amount
of functionality that needs to be provided by the glue code. When encountering
such problems, please send a bug report with an example of the source that
causes the problem to the author.


%------------------------------------------------------------------------------


\section{Restrictions}

\begin{itemize}
  \item \prog{tcsim} only includes a small part of the network stack, and
    does not support full routing or firewalling. Therefore, the \name{route}
    classifier is not available in \prog{tcsim}, and the usefulness of the
    \name{fw} classifier is limited.
  \item \prog{tc} bugs may crash \prog{tcsim}, e.g. try running
    \verb"tc class show dev eth0" at the end of \path{../examples/prio+rsvp}
\end{itemize}

Known bugs are listed in the separate file \url{tcng/tcsim/BUGS},
included in the \prog{tcng} distribution.


%==============================================================================
