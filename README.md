Web Server

This program is a simple multi-threaded Web server written in C.

The server program will receive two arguments: 1) the port number it should
listen on for incoming connections, and 2) the directory out of which it
will serve files. For example:

$ ./homework5 8080 WWW

This command will tell your Web server to listen for connections on port
8080 and serve files out of the WWW directory.

If you're hosting on your localhost, you can check for functionality
by visiting localhost:PORT_NUMBER/PATH on your web browser.

Replace PORT_NUMBER with the port number you inputted above.
Replace PATH with any of the files in the WWW directory.
