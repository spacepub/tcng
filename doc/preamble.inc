%
% preamble.inc - t2x definitions and copyright statement
%
% Written 2001-2004 by Werner Almesberger
% Copyright 2001,2002 Bivio Networks, Network Robots
% Copyright 2002-2004 Werner Almesberger
%

%%beginskip
\parindent=0pt
\parskip=4pt
\tolerance=9000

\def\meta#1{{\it #1\/}} % ... <blah> ...
\def\raw#1{{\tt #1}}    % ...  raw output  ...
\def\emphasize#1{{\bf #1}} % ... _don't_ ...
\def\name#1{{\bf #1}}    % ... "name" ...
\def\uname#1{{\tt #1}}    % ... "name" ..., with multiple underscores
\def\path#1{{\tt #1}}    % ... /foo/bar ...
\def\prog#1{#1}    % ... tcsim ...
%%def\\meta{([^{}]*)}=<$1>
%%def\\raw{(([^{}]|\\{)*)}=$1
%%def\\emphasize{([^{}]*)}=\\_$1\\_
%%def\\name{([^{}]*)}=``$1''
%%def\\uname{([^{}]*)}=``$1''
%%def\\path{([^{}]*)}=$1
%%def\\prog{([^{}]*)}=$1
%%def\\url{([^{}]*)}=\\path{$1}
\def\todo{{\large\bf TO DO}}
%%def\\todo\b=TODO
%%def\\tt\b=
%%def\\em\b=
%%def\\small\b=
%%def\\tiny\b=
%%def\\scriptsize\b=

% kludge
%%def\\appendix=

%------------------------------------------------------------------------------


\maketitle
\pagestyle{plain}
%%endskip

%------------------------------------------------------------------------------


%%beginskip
{\huge\bf Copyright and acknowledgements}
\vskip5mm
%%endskip
%:\chapter*{Copyright and acknowledgements}

Written 2001--2004 by Werner Almesberger

Copyright 2001 EPFL-ICA \\
Copyright 2001--2004 Werner Almesberger \\
Copyright 2001,2002 Bivio Networks \\

This document program is distributed under version 2 of the General
Public License. See \url{http://www.gnu.org/copyleft/} for details.

This work started 2001 at EPFL ICA, and continued 2001--2002 for
Bivio Networks Inc. (former Network Robots).

Testing and tcng language design also by Milena Mondini, U. of Napoli
``External'' interface and related elements jointly designed with Jacob
Teplitsky, Bivio Networks.

The author thanks Jacob Teplitsky and Ronald Murphy for reviewing
this document and for making many valuable suggestions for improvements.


%------------------------------------------------------------------------------


%%beginskip
\tableofcontents
%%endskip
