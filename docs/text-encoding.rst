Text Encoding
=============

This page serves as concept for adding additional CopyQ command line
switch to print and read texts in UTF-8 (i.e. without using system
encoding).

Every time the bytes are read from a command (standard output or
arguments from client) the input is expected to be either just series of
bytes or text in system encoding (possibly Latin1 on Windows). But
texts/strings in CopyQ and in clipboard are UTF-8 formatted (except some
MIME types with specified encoding).

When reading system-encoded text (MIME starts with "text/") CopyQ
re-encodes the data from system encoding to UTF-8. That's not a problem
if the received data is really in system encoding. But if you send data
from Perl with the UTF-8 switch, CopyQ must also know that UTF-8 is used
instead of system encoding.

The same goes for other way. CopyQ sends texts back to client or to a
command in system encoding so it needs to convert these texts from
UTF-8.

As for the re-encoding part, Qt 5 does nice job transforming characters
from UTF-8 but of course for lot of characters in UTF-8 there is no
alternative in Latin1 and other encodings.
