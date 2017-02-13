%
% tcc.inc - Traffic Control Compiler
%
% Written 2001,2002,2004 by Werner Almesberger
% Copyright 2001 EPFL-ICA
% Copyright 2001,2002 Bivio Networks, Network Robots
% Copyright 2002,2004 Werner Almesberger
%


%------------------------------------------------------------------------------


\prog{tcng} reads traffic control specifications in the \prog{tcng}
language and generates command or files for a number of mechanisms
that are used to implement traffic control functionality.

We describe the invocation of \prog{tcng}, and give an overview how
\prog{tcng} interacts with the output processors of the respective
targets.


%------------------------------------------------------------------------------


\section{Invocation}

The number of \prog{tcng}'s command-line options may be intimidating at
first. \prog{tcng} is typically invoked from scripts that provide all
the default options needed in the specific scenario. In interactive use,
the most common options are the target selectors \raw{-t} and \raw{-x}.

If only using traffic control configured by \name{iproute2/tc}, no
command line options are needed.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Usage}
\label{tccusg}

\raw{tcng}
$[$\raw{-c}$]$
$[$\raw{-d} $\ldots]$
$[$\raw{-E}$]$
$[$\raw{-i \meta{default\_interface}}$]$
$[$\raw{-l \meta{location\_file}}$]$
$[$\raw{-n}$]$
$[$\raw{-q}$]$
$[$\raw{-r}$]$
$[$\raw{-w}$]$
$[$\raw{-W$[$no$]$\meta{condition}}$]$
$[$\raw{-O$[$no$]$\meta{option}}$]$
$[$\raw{-x \meta{element}:\meta{ext\_target}} $\ldots]$
$[$\raw{-t $[$no$]$\meta{target}} $\ldots]$
$[$\raw{-u \meta{var\_use\_file}}$]$
$[$\raw{-X\meta{phase},\meta{argument}}$]$
$[$\meta{cpp\_option}$]$
$[$\meta{file}$]$

\raw{tcng} \raw{-V}

\begin{description}
  \item[\raw{-c}] only check validity of input, don't build a
    configuration. If requested, the location file and the variable use
    file are also generated when using \raw{-c}
  \item[\raw{-d}] increase debugging level
  \item[\raw{-E}] only run \prog{cpp}, and send its output to standard
    output. This is useful for separately running files through \prog{cpp},
    while using the default includes added by \prog{tcng}.
  \item[\raw{-i \meta{default\_interface}}] interface to use if none is
    specified in the description file. See section \ref{inthist} for
    further details.
  \item[\raw{-l \meta{location\_file}}] write a list of source code locations
    of traffic control elements to the specified file. See section
    \ref{locfile} for details. Using the special file name \name{stderr} sends
    the output to standard error.
  \item[\raw{-n}] do not include \name{default.tc}. By default, \prog{tcng}
    includes this file, which in turn includes the files described
    in section \ref{tcnginc}. This can be undesirable, e.g. if operating in
    a non-TCP/IP context, or if using a different default include file with
    application-specific definitions. In the latter case, the following
    options should be used: \\
    \verb"tcng" $\ldots$ \verb"-n" $\ldots$ \verb"-Xp,--include"
      \verb"-Xp,/"\meta{directory}\verb"/"\meta{file} $\ldots$
  \item[\raw{-O$[$no$]$\meta{option}}] enable or disable the specified
    optimization. The following optimizations are recognized:
    \begin{description}
      \item[\name{cse}] common subexpression elimination
      \item[\name{ne}] turn \raw{!=} into multiple \raw{==}s
      \item[\name{prefix}] generate prefix matches instead of bit tests
    \end{description}
    By default, all optimizations except \name{cse} are turned off.
  \item[\raw{-q}] quiet, produce terse output
  \item[\raw{-r}] remove old queuing disciplines before adding new
    ones (\name{tc} only)
  \item[\raw{-t $[$\meta{elem\raw{:}}$][$no$]$\meta{target}} $\ldots$]
    enable or disable target (see section \ref{targets}).
    The only element currently supported is
    \name{if}. Supported targets are \name{all}, \name{tc} (default), \name{c},
    and \name{ext}. The \raw{-t} options can be repeated to enable or disable
    multiple targets.
  \item[\raw{-u \meta{var\_use\_file}}] for each variable, write its name and
    value to the specified file. See section
    \ref{varfile} for details. Using the special file name \name{stderr} sends
    the output to standard error.
  \item[\raw{-V}] print version number and exit
  \item[\raw{-w}] suppress all warnings
  \item[\raw{-W$[$no$]$\meta{condition}}] enable or disable issuing a warning
    message on the specified condition. The following conditions are
    recognized:
    \begin{description}
      \item[\name{constpfx}] warn if using the \verb"/" (mask) operator on a
	constant IPv4 or IPv6 address, as in \verb"ip_src == 1.2.3.0/24",
        which should probably be \verb"ip_src/24 == 1.2.3.0"
      \item[\name{expensive}]  generate warnings when encountering
	``expensive'' constructs in \name{if} classifier.
	A construct is considered ``expensive'' if processing it
	may take \prog{tcng} an unusual amount of time.
	Operations like negation, certain tests for non-equality, or
	static classification that follows metering, are ``expensive''.

	Because \name{expensive} may be overly pessimistic,
	\name{exppostopt} should be used whenever possible.
      \item[\name{experror}] turns warnings from \name{expensive} or
	\name{exppostopt} into fatal errors. Setting \name{experror} without
	also setting \name{expensive} or \name{exppostopt} has no effect.
      \item[\name{explicit}] warn if a class if explicitly specified for the
        inner queuing discipline of \name{dsmark}, \name{egress}, or
	\name{ingress}
      \item[\name{exppostopt}] like \name{expensive},
	but test for ``expensive'' constructs
	after trying to optimize the expression, which may eliminate some
	apparently expensive constructs. \name{exppostopt} can only
	indicate in which \name{if} classifier the expression occurred,
	not in which selector of that classifier.
	\name{expensive} has no effect if \name{exppostopt} is set.

	Note that neither
	\name{expensive} nor \name{exppostopt} are useful when using
	\name{nocombine} (see section{barriers}).
      \item[\name{redefine}] warn if re-defining variables
      \item[\name{truncate}] warn if truncating values, e.g. when converting
        a floating-point number to an integer
      \item[\name{unused}] report unused variables
    \end{description}
    By default, all warnings except \name{explicit} and \name{unused} are
    turned off.
  \item[\raw{-x \meta{element}:\meta{ext\_target}} $\ldots$]
    register external target (see section \ref{targets}). The \raw{-x} option
    can be repeated to register multiple external targets.
  \item[\raw{-X\meta{phase},\meta{argument}}] verbatim argument for specific
     build phase. See section \ref{phtcc} for details.
  \item[\meta{cpp\_option}] one of the following options for the C
    pre-processor: \raw{-I\meta{dir}},
    \raw{-D\meta{name}}$[$\raw{=\meta{value}}$]$, or \raw{-U\meta{name}}
\end{description}

\prog{tcng} also recognizes a number of undocumented options, such as
\raw{-B} and \raw{-N}. These options usually enable experimental
features, like alternative algorithms for processing \name{if}
expressions, and they are not recommended for normal use of \prog{tcng}.
Changes to undocumented options are mentioned in \prog{tcng}'s change
log (file \path{CHANGES}).


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Phases underneath \prog{tcng}}
\label{phtcc}

\prog{tcng} and its companion program \prog{tcc-module} do not only take
command-line options for themselves, but they can also pass command-line
options to programs they invoke. 

Each such program invocation is called a ``phase'', and has a one-letter
name assigned to it. Options are passed to that specific phase, with the
command-line option \raw{-X\meta{phase},\meta{option}}

The phases and their names are shown in figure \ref{phtccfig}.

\begin{figure}[ht]
\begin{center}
\epsfig{file=phtcc.eps}
\end{center}
\caption{Parameter passing to phases underneath \prog{tcng}.}
\label{phtccfig}
\end{figure}

Note the two \prog{cpp} phases of \prog{tcsim} and \prog{tcng} are 
identified by a upper and lower case ``p'', respectively.

For the external interface, parameters can also be sent to a specific
external program (\name{tcc-ext-\meta{name}}) with the phase name
\raw{x\meta{name}}.


%------------------------------------------------------------------------------


\section{Targets}
\label{targets}

\prog{tcng} can generate output for various ``targets'', which then
perform the actual configuration. Targets are selected with the
command-line options \raw{-t} and \raw{-x}.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{The ``tc'' target}

This is the default target.

\prog{tcng} outputs commands for the
\prog{tc} command, which configures the software-based traffic
control subsystem in the Linux kernel.

These commands are written to standard output, and can be processed
by a shell. Note that \prog{tcng} may detect errors only after it has
begun generating output, so in order to avoid partial configuration,
the output should be stored in a temporary file, and only passed to
the shell after \prog{tcng} has completed.

Example:

\begin{verbatim}
#!/bin/sh
tcng ... >tmpfile || { rm -f tmpfile; exit 1; }
. tmpfile
rm -f tmpfile
\end{verbatim}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{The ``C'' target}

The ``C'' target is enabled with the command-line option \raw{-tc}.

Instead of using the \name{u32} classifier for the \name{if} construct,
``C'' writes the corresponding expression in the C language to a file,
and generates a kernel module for it. The resulting custom-built
classifier should be much more efficient than a classifier using
\name{u32}.

The commands to load the kernel module are issued on standard output,
along with the \name{tc} commands, so the same scripts as for the
``tc'' target can be used.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{The external target}

The external target is enabled with the command-line option
\raw{-xall:\meta{name}} or
\raw{-xif:\meta{name}}, where \meta{name} is the corresponding external
program.

The external interface is described in chapter \ref{chpext}. As
discussed further in that chapter, \prog{tcng} may build a hybrid
configuration combining \name{tc} and elements from the external
interface with \raw{-xif:\meta{name}}, while it uses only the
external interface with \raw{-xall:\meta{name}}.

Example:

The following configuration

\begin{verbatim}
egress {
    drop if tcp_sport != PORT_SSH;
}
\end{verbatim}

yields, when invoking \prog{tcng} with \raw{-xif:err} (the external
program \path{tcc-ext-err} simply echoes the configuration data
from \prog{tcng} on standard error):

\name{tc} commands on standard output:
{\small
\begin{verbatim}
tc qdisc add dev eth0 handle 1:0 root dsmark indices 1 default_index 0
\end{verbatim}
}

External configuration on standard error:
\begin{verbatim}
# 7700
block eth0 egress
offset 100 = 0+(0:4:4 << 5)
action 0 = drop
action 1 = unspec
match 0:72:8=0x06 100:0:16=0x0016 action 1
match 0:72:8=0x06 action 0
match action 1
# END
\end{verbatim}

When invoking \prog{tcng} with \raw{-xall:err}, we only get an
external configuration, on standard error:

\begin{verbatim}
# fc00
offset 100 = 0+(0:4:4 << 5)
block eth0 egress
qdisc 1 = dsmark indices 1
action 0 = drop
action 1 = unspec
match 0:72:8=0x06 100:0:16=0x0016 action 1
match 0:72:8=0x06 action 0
match action 1
# END
\end{verbatim}

Note: the comment lines (\verb"# ...") are inserted by \path{tcc-ext-err}
and are not part of the configuration data passed from \prog{tcng}.


%------------------------------------------------------------------------------


\section{File formats}

\prog{tcng} generates a number of different output files. The target-specific
files have already been described in section \ref{targets}, with more details
on the external interface in section \ref{extcfg}. In this section, we
describe the files which do not depend on a specific target.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Location map}
\label{locfile}

When invoked with the option \raw{-l \meta{location\_file}}, \prog{tcng} writes
the source code locations of traffic control elements to the specified file.

The file format is as follows:

\meta{type} \meta{identifier} \meta{tag} \meta{file\_name} \meta{line\_number}

\begin{description}
  \item[\meta{type}] is the type of the element (see below)
  \item[\meta{identifier}] is a string that uniquely identifies the element
  \item[\meta{tag}] is a user-provided identifier (``--'' if no identifier was
    provided)
  \item[\meta{file\_name}] is the name of the source file containing the
    definition of the element (``--'' if no file name is available; note that
    \prog{tcng} uses the name \raw{<stdin>} for standard input)
  \item[\meta{line\_number}] is the approximate line number
\end{description}

The format of the identifier depends on the type of the element:

\begin{tabular}{ll}
  Type name & Identifier string \\
  \hline
  \name{device}  & \meta{interface} \\
  \name{qdisc}   & \raw{\meta{interface}:\meta{qdisc}} \\
  \name{class}   & \raw{\meta{interface}:\meta{qdisc}:\meta{class\_number}} \\
  \name{filter}  & \raw{\meta{interface}:\meta{qdisc}:\meta{class\_number}%
:\meta{priority}} \\
  \name{tunnel}  & \raw{\meta{interface}:\meta{qdisc}:\meta{class\_number}%
:\meta{priority}:\meta{tunnel\_number}} \\
  \name{element} & \raw{\meta{interface}:\meta{qdisc}:\meta{class\_number}%
:\meta{priority}:\meta{element\_index}} \\
  \name{police}  & \meta{bucket\_number} \\
\end{tabular}

\meta{qdisc} is the queuing discipline number for egress, or
\name{ingress} at ingress. For \name{filter} and \name{element},
\meta{class\_number} is an empty string if the filter is rooted at the
queuing discipline. \meta{element\_index} is the zero-based number of the
element on that filter. Note that buckets are also identified as
policers.

Programs reading location maps should not make any assumption about
the order in which entries are written.

Future versions of \prog{tcng} may add more fields to a map line, so any
programs reading location maps should not assume that a line ends
after the line number.

Example:

\begin{verbatim}
dev "eth0" {
    prio (1) {
        class (1,"aTag")
            if 1
        {
            fifo (2,tag "another_tag");
        }
    }
}
\end{verbatim}

yields the following location map:

\begin{verbatim}
device eth0 - <stdin> 1
qdisc eth0:1 - <stdin> 2
class eth0:1:1 aTag <stdin> 3
qdisc eth0:2 another_tag <stdin> 6
filter eth0:1::1 - <stdin> 2
element eth0:1::1:0 - <stdin> 4
\end{verbatim}

Note that \name{if} is converted into a filter with elements.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Variable use list}
\label{varfile}

When invoked with the option \raw{-u \meta{variable\_use\_file}},
\prog{tcng} writes the names of all variables and their content to the
specified file.

Each line is in one of the following formats:

\raw{\{ \meta{type} \meta{identifier}}

\raw{\{}

\raw{\}}

\raw{\$\meta{name} = \meta{number}}

\raw{\$\meta{name} = \meta{number} \meta{unit}}

\raw{\$\meta{name} = expression}

\raw{\$\meta{name} = \meta{type} \meta{identifier}}

\meta{name} is the variable name. \meta{number} is an integer or a
floating-point number. \meta{unit} is the unit of measurement, without
prefix. Units are described in section \ref{units}.

For variables containing non-constant expressions, simply the word
\raw{expression} is output as the value. Future versions of \prog{tcng}
may add further information after this keyword.

For variables containing traffic control elements, the format is
identical to the first two fields in the location map entry of that
element. Note that only queuing disciplines, classes, filters, buckets,
and policers can be assigned to variables.

The beginning of a scope is indicated with an opening curly brace
followed by the identifier of the element that started the scope.
Scopes created by nesting a block directly within another block have
no element identifying the scope and are denoted by a line containing
only the opening curly brace. Likewise, the end of a scope is
indicated with a closing curly brace.

Example:

\begin{verbatim}
prio {
    $foo = 42
}
\end{verbatim}

yields the following variable use list:

\begin{verbatim}
{ device eth0
{ qdisc eth0:1
$foo = 42
}
}
\end{verbatim}

Note that the scope for the default device is automatically generated.
As a special case, the scope of an unnumbered class is marked with
the identifier \raw{\meta{interface}:\meta{qdisc}:unnumbered}

Programs reading a variable use list file should not make any
assumption about the order in which entries within the same scope are
written. Variables with the same name will appear once per distinct
variable, within the respective scope.

Note that forward-declared variables are placed in the scope of their
assignment, and not the scope of their declaration.

Future versions of \prog{tcng} may include whitespace before dollar signs
and curly braces. Furthermore, the syntax
\raw{\$\meta{name} = forward}
is reserved for possible future use.


%------------------------------------------------------------------------------


\section{Adding new traffic control elements to \prog{tcng}}

While it is possible to add new traffic control elements to \prog{tcng},
this is currently somewhat complicated. In this section, we outline the
general procedure. For further details, the source code of existing
traffic control elements provides the best reference.

Before adding new elements to \prog{tcng}, the possibility of re-using
existing elements should be considered. It is frequently easier to
provide a set of macros that adapt an existing element, than
implementing new elements from scratch.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Adding new queuing disciplines}

If the new queuing discipline requires new parameters, the following
files need to be changed:

\begin{description}
  \item[\path{tcc/Parameters}] defines name and type of the parameters.
    Various include files are automatically generated from this file.
  \item[\path{tcc/tcng.l}] parses the parameter name and returns a
    suitable token
  \item[\path{tcc/tcng.y}] declares the token (in a \verb"%token"
    line), and processes the parameter in the \name{parameter\_expression}
    rule
\end{description}

For the queuing discipline, a file called \path{q\_\meta{name}.c}
needs to be written, that provides the functions and data structures
that define how the queuing discipline is processed.

In addition to writing the \path{q\_\meta{name}.c} file, the following
files need to be changed:

\begin{description}
  \item[\path{Makefile}] include \path{q\_\meta{name}.c} in \name{FILES}
  \item[\path{tcc/Makefile}] include \path{q\_\meta{name}.o} in \name{OBJS}
  \item[\path{tcc/qdisc.h}] add a declaration of the descriptor
  \item[\path{tcc/tcng.l}] parses the queuing discipline name and returns
    a suitable token
  \item[\path{tcc/tcng.y}] declares the token (in a \verb"%token"
    line), and processes the queuing discipline in the \name{qdisc\_name}
    rule
\end{description}

Also, new elements should be exercised by the regression tests. Adding
a new test case involves the following files:

\begin{description}
  \item[\path{tests/\meta{name}}] the file containing the actual tests
  \item[\path{Makefile}] include \path{tests/\meta{name}} in \name{FILES}
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Adding new filters}

The procedures for adding parameters and regression tests are the same
for filters as for queuing disciplines.

For the filter, a file called \path{f\_\meta{name}.c}
needs to be written, that provides the functions and data structures
that define how the filter is processed. Furthermore, the following
files need to be changed:

\begin{description}
  \item[\path{Makefile}] include \path{f\_\meta{name}.c} in \name{FILES}
  \item[\path{tcc/Makefile}] include \path{f\_\meta{name}.o} in \name{OBJS}
  \item[\path{tcc/filter.h}] add a declaration of the descriptor
  \item[\path{tcc/tcng.l}] parses the filter name and returns
    a suitable token
  \item[\path{tcc/tcng.y}] declares the token (in a \verb"%token"
    line), and processes the filter in the \name{filter\_name}
    rule
\end{description}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Adding new meters}

In general, new metering functions are implemented using the \name{bucket}
primitive. No change to \prog{tcng} is needed for this.

The file \path{tcng/tcc/meters.tc.in} contains the
definitions of the meters which are part of the \prog{tcng} language.

\path{meters.tc.in} uses a special pseudo preprocessor directive called
\verb"#map". This directive is converted by \path{tcng/tcc/tcc\_var2fix.pl}
to a set of macros for elements with a \prog{tcng}-style parameter list.
Please consult the source code of \path{tcng/tcc/tcc\_var2fix.pl} for a
detailed description of these macros.


%==============================================================================
