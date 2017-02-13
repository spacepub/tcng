%
% param.inc - Parameter reference
%
% Written 2002,2004 by Werner Almesberger
% Copyright 2002 Bivio Networks, Network Robots
% Copyright 2002,20004 Werner Almesberger
%

%%beginskip
\def\R{$\bullet$}
\def\O{$\circ$}
\def\.{$\cdot$}
%%endskip
%%def\\R=*
%%def\\O=o
%%def\\\.=.

Note that this appendix only lists how \prog{tcng} expects parameters
to be used. This is not meant to be a description of the semantics of
parameters, or the underlying algorithms. For this type of information,
please consult the \cite{LARTC} Web site, and \cite{ALM99}.


% -----------------------------------------------------------------------------


\section{General parameters}

The following parameters are available on all queuing disciplines,
classes, filters, and filter elements:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{pragma}		& list of strings & Implementation-specific data \\
  \name{tag}		& string & User-provided identification tag \\
\end{tabular}
\end{center}

Both parameters are optional.


% -----------------------------------------------------------------------------


\section{Queuing discipline parameters}

The following table shows which parameters are supported by which queuing
discipline, and whether they are optional (\O) or required (\R):

\begin{center}
\small
\begin{tabular}{l|ccccccccc}
  Parameter		& \name{cbq} &
			       \name{dsmark} &
				    \name{fifo} &
					 \name{gred} &
					     \name{htb} &
						  \name{prio} &
						       \name{red} &
							    \name{sfq} &
								 \name{tbf} \\
  \hline
  \name{avpkt}		& \R & \. & \. & \. & \. &\. & \R & \. & \. \\
  \name{bands}		& \. & \. & \. & \. & \. &\O & \. & \. & \. \\
  \name{bandwidth}	& \R & \. & \. & \R & \. &\. & \R & \. & \. \\
  \name{burst}		& \. & \. & \. & \. & \. &\. & \R & \. & \R \\
  \name{default\_index}	& \. & \O & \. & \. & \. &\. & \. & \. & \. \\
  \name{ecn}		& \. & \. & \. & \. & \. &\. & \O & \. & \. \\
  \name{ewma}		& \O & \. & \. & \. & \. &\. & \. & \. & \. \\
  \name{grio}		& \. & \. & \. & \O & \. &\. & \. & \. & \. \\
  \name{indices}	& \. & \O & \. & \. & \. &\. & \. & \. & \. \\
  \name{limit}		& \. & \. & \O & \. & \. &\. & \R & \. & \R \\
  \name{max}		& \. & \. & \. & \. & \. &\. & \R & \. & \. \\
  \name{min}		& \. & \. & \. & \. & \. &\. & \R & \. & \. \\
  \name{mpu}		& \O & \. & \. & \. & \. &\. & \. & \. & \. \\
  \name{mtu}		& \. & \. & \. & \. & \. &\. & \. & \. & \O \\
  \name{peakrate}	& \. & \. & \. & \. & \. &\. & \. & \. & \O \\
  \name{perturb}	& \. & \. & \. & \. & \. &\. & \. & \O & \. \\
  \name{priomap}	& \O & \. & \. & \. & \. &\O & \. & \. & \. \\
  \name{probability}	& \. & \. & \. & \. & \. &\. & \O & \. & \. \\
  \name{quantum}	& \. & \. & \. & \. & \. &\. & \. & \O & \. \\
  \name{r2q}		& \. & \. & \. & \. & \O &\. & \. & \. & \. \\
  \name{rate}		& \R & \. & \. & \. & \. &\. & \. & \. & \R \\
  \name{set\_tc\_index} & \. & \O & \. & \. & \. &\. & \. & \. & \. \\
\end{tabular}
\end{center}

The \name{ingress} queuing discipline has no parameters and has
therefore been omitted from the above table.

Units and meaning of the parameters are described below:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{avpkt}		& Bytes & Average packet size \\
  \name{bands}		& integer & Number of bands \\
  \name{bandwidth}	& bps	& Interface bandwidth \\
  \name{burst}		& Bytes & Burst size, typically $>=$ MTU \\
  \name{default\_index}	& integer & Index if no class matches \\
  \name{ecn}		& flag	& Set CE instead of dropping \cite{RFC3168} \\
  \name{ewma}		& integer & EWMA weight is $2^{-ewma}$ \\
  \name{grio}		& flag	& Enable RIO-like buffer sharing \\
  \name{indices}	& integer & Indices used for changing the DS field\\
  \name{limit}		& Bytes & Queue size limit in bytes \\
  \name{limit}		& pcks	& Queue size limit in packets \\
  \name{max}		& Bytes & Minimum threshold ($min\_th$) \\
  \name{min}		& Bytes & Maximum threshold ($max\_th$) \\
  \name{mpu}		& Bytes & Minimum policed unit \\
  \name{mtu}		& Bytes & Maximum transmission unit \\
  \name{peakrate}	& bps	& Peak data rate \\
  \name{perturb}	& secs	& Perturbation interval \\
  \name{priomap}	& list of classes & Default class map \\
  \name{probability}	& floating-point & Maximum drop probability \\
  \name{quantum}	& Bytes & Maximum burst when scheduled \\
  \name{quantum}	& secs  & Burst duration when scheduled 
				  (\name{HTB} only) \\
  \name{rate}		& bps	& Nominal data rate \\
  \name{r2q}		& integer & Rate to quantum conversion ratio
				    (deprecated) \\
  \name{set\_tc\_index} & flag	& Copy DS field to \name{tc\_index} \\
\end{tabular}
\end{center}

In addition to the parameters described in this section, queuing
disciplines with classes also allow default values for class parameters
to be specified at the queuing discipline. They are listed in the next
section.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Differences from \prog{tc} for \name{fifo}}

Unlike \prog{tc}, which has two types of FIFO queuing disciplines -- one
with the limit expressed in bytes(\name{bfifo}), and one with the limit
expressed in packets (\name{pfifo}) -- \prog{tcng} only uses one queuing
discipline (\name{fifo}), but allows the value of the \name{limit}
parameter to be either in bytes or in packets.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Differences from \prog{tc} for \name{htb}}

For setting the quantum, \prog{tcng} offers a better mechanism than
\prog{tc}: instead of using the slightly obscure \name{r2q} parameter,
users can directly specify the quantum as the time during which the
class can monopolize the link, which is then converted to the
corresponding number of bytes. For compatibility, also \name{r2q}
is supported.


% -----------------------------------------------------------------------------


\section{Class parameters}

The following table shows which class parameters are supported by which
queuing discipline, and whether they are optional (\O) or required (\R):

\begin{center}
\small
\begin{tabular}{l|cccc}
  Parameter		& \name{cbq} &
			       \name{dsmark} &
				    \name{gred} &
					\name{htb} \\
  \hline
  \name{allot}		& \R & \. & \. & \. \\
  \name{avpkt}		& \O & \. & \R & \. \\
  \name{bandwidth}	& \R & \. & \. & \. \\
  \name{bounded}	& \O & \. & \. & \. \\
  \name{burst}		& \O & \. & \R & \O \\
  \name{cburst}		& \. & \. & \. & \O \\
  \name{ceil}		& \. & \. & \. & \O \\
  \name{default}	& \. & \. & \O & \O \\
  \name{isolated}	& \O & \. & \. & \. \\
  \name{ewma}		& \O & \. & \. & \. \\
  \name{limit}		& \O & \. & \R & \. \\
  \name{mask}		& \. & \O & \. & \. \\
  \name{max}		& \O & \. & \R & \. \\
  \name{maxburst}	& \O & \. & \. & \. \\
  \name{min}		& \O & \. & \R & \. \\
  \name{minburst}	& \O & \. & \. & \. \\
  \name{mpu}		& \O & \. & \. & \. \\
  \name{prio}		& \O & \. & \O & \O \\
  \name{probability}	& \. & \. & \O & \. \\
  \name{rate}		& \R & \. & \. & \R \\
  \name{value}		& \. & \O & \. & \. \\
  \name{weight}		& \O & \. & \. & \. \\
\end{tabular}
\end{center}

Classes of \name{ingress} and \name{prio} have no parameters and have
therefore been omitted from the above table.

Units and meaning of parameters we haven't already described in the queuing
discipline section:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{allot}		& Bytes	& Sending credit when scheduled \\
  \name{bounded}	& flag	& Class may not ``borrow'' bandwidth \\
  \name{cburst}		& Bytes & Burst exceeding \name{ceil} rate \\
  \name{ceil}		& bps   & Maximum bandwidth a class can use \\
  \name{default}	& flag	& Use this class if DP unknown or if
				  classification fails \\
  \name{isolated}	& flag	& Neither ``borrow'' or ``lend'' bandwidth \\
  \name{mask}		& integer & DS field mask \\
  \name{maxburst}	& pcks	& Maximum burst while over-limit \\
  \name{minburst}	& pcks	& Minimum burst when scheduled \\
  \name{prio}		& integer & Scheduling priority \\
  \name{value}		& integer & New DS field value \\
  \name{weight}		& bps	& Scheduling weight \\
\end{tabular}
\end{center}


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Differences from \prog{tc} for \name{gred}}

While \prog{tc} has a \name{default} parameter at the queuing discipline,
``pointing'' to the class to select if the drop precedence value is too
large, \prog{tcng} uses a flag parameter, which is also called \name{default},
at the class for the same purpose.


% - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


\subsection{Differences from \prog{tc} for \name{htb}}

Like with \name{gred}, \name{tcng} uses the parameter \name{default} to
mark the default class, instead of ``pointing'' to the class from the
queuing discipline.


% -----------------------------------------------------------------------------


\section{Filter parameters}

The following table shows which parameters are supported by which filter, 
and whether they are optional (\O) or required (\R):

\begin{center}
\small
\begin{tabular}{l|cccc}
  Parameter		& \name{fw} &
			       \name{route} &
				    \name{rsvp} &
				        \name{tcindex} \\
  \hline
  \name{fall\_through}	& \. & \. & \. & \O \\
  \name{hash}		& \. & \. & \. & \O \\
  \name{mask}		& \. & \. & \. & \O \\
  \name{pass\_on}	& \. & \. & \. & \O \\
  \name{protocol}	& \O & \O & \O & \O \\
  \name{shift}		& \. & \. & \. & \O \\
\end{tabular}
\end{center}

Units and meaning of the parameters are described below:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{fall\_through}	& flag	& If no match, calculate index \\
  \name{hash}		& integer & Hash table size \\
  \name{mask}		& integer & DS field value mask \\
  \name{pass\_on}	& flag	& Negation of \name{fall\_through} \\
  \name{protocol}	& Ether protocol & Network layer protocol \\
  \name{shift}		& integer & Number of bits to right-shift DS field \\
\end{tabular}
\end{center}

The unit ``Ether protocol'' is special: the value can either be an integer,
or a string containing one of the names \name{ip}, \name{ipv4}, or
\name{ipv6}.

In addition to the parameters described in this section, filters
also allow default values for filter element parameters to be specified
at the filter. They are listed in the next section.


% -----------------------------------------------------------------------------


\section{Filter element parameters}

The following table shows which element parameters are supported by which
filter, and whether they are optional (\O) or required (\R):

\begin{center}
\small
\begin{tabular}{l|cc}
  Parameter		& 
			  \name{route} &
			       \name{rsvp} \\
  \hline
  \name{ah}		& \. & \O \\
  \name{dport}		& \. & \O \\
  \name{dst}		& \. & \R \\
  \name{esp}		& \. & \O \\
  \name{from}		& \O & \. \\
  \name{fromif}		& \O & \. \\
  \name{ipproto}	& \. & \O \\
  \name{order}		& \O & \. \\
  \name{sport}		& \. & \O \\
  \name{src}		& \. & \O \\
  \name{to}		& \O & \. \\
\end{tabular}
\end{center}

Elements of \name{fw} and \name{tcindex} have no parameters and have
therefore been omitted from the above table.

Units and meaning of parameters we haven't already described in the filter
section:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{ah}		& integer & SPI in AH header \cite{RFC2402} \\
  \name{dport}		& port    & Destination port number \\
  \name{dst}		& host    & Destination IP address \\
  \name{esp}		& integer & SPI in ESP header \cite{RFC2406} \\
  \name{from}		& integer & Incoming realm \\
  \name{fromif}		& string  & Incoming interface name \\
  \name{ipproto}	& IP protocol & Transport layer protocol \\
%%  \name{order}		& integer & \\ MUST BE ZERO ?!?
  \name{sport}		& port    & Source port number \\
  \name{src}		& host    & Source IP address \\
  \name{to}		& integer & Outgoing realm \\
\end{tabular}
\end{center}

The units ``host'', ``port'', and ``IP protocol'' are special: the
value can either be an integer, or a string containing the name of a
host, a service, or a transport-layer protocol, respectively. If a name
is given, \prog{tcng} invokes the corresponding resolver function of the
host to obtain the numeric value.


% -----------------------------------------------------------------------------


\section{Tunnel parameters}

Tunnels only take a single parameter, which is required:

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{skip}		& Bytes & Number of header bytes to remove \\
\end{tabular}
\end{center}


% -----------------------------------------------------------------------------


\section{Policer and bucket parameters}

The following table shows which element parameters are supported by buckets
and policers, and whether they are optional (\O) or required (\R):

\begin{center}
\small
\begin{tabular}{l|cc}
  Parameter		& 
			  \name{bucket} &
			       \name{policer} \\
  \hline
  \name{avrate}		& \. & \O /\R \\
  \name{burst}		& \R & \R /\O \\
  \name{mpu}		& \O & \O \\
  \name{mtu}		& \. & \O \\
  \name{overflow}	& \O & \O \\
  \name{peakrate}	& \. & \O \\
  \name{rate}		& \R & \R \\
\end{tabular}
\end{center}

For policers, either \name{avrate} or \name{burst} must be present.

\begin{center}
\begin{tabular}{lll}
  Parameter  & Unit  & Description \\
  \hline
  \name{avrate}		& bps   & Average policed rate \\
  \name{burst}		& Bytes & Burst size, typically $>=$ MTU \\
  \name{mpu}		& Bytes & Minimum policed unit \\
  \name{mtu}		& Bytes & Maximum transmission unit \\
  \name{overflow}	& bucket & Bucket to receive excess credit \\
  \name{peakrate}	& bps	& Peak data rate \\
  \name{rate}		& bps	& Sustainable data rate \\
\end{tabular}
\end{center}


%==============================================================================
