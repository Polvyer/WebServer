#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>

#define DIRECTORY_LISTING_MAX_CHAR 1013 // can get overflow if have a large directory and not enough characters
#define BACKLOG (10)

void serve_request(int, char*);

char * william; // Threading

char * request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * text_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n\r\n";

char * pdf_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: application/pdf; charset=UTF-8\r\n\r\n";

char * gif_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/gif; charset=UTF-8\r\n\r\n";

char * png_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/png; charset=UTF-8\r\n\r\n";

char * jpg_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/jpeg; charset=UTF-8\r\n\r\n";

char * not_found_str = "HTTP/1.0 404 Not found\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
        "<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";

// snprintf(output_buffer,4096,index_hdr,filename,filename);


char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></html>";
//------------------------------------------------------------------
char* get_directory_contents(char* directory_path, int *flag, char* rf)
{
  char* directory_listing = NULL;
  char* index_listing = NULL;
  char* dir_listing = NULL;
  
  // open directory path up 
  DIR* path = opendir(directory_path);

  // check to see if opening up directory was successful
  if(path != NULL)
  {
      directory_listing = (char*) malloc(sizeof(char)*DIRECTORY_LISTING_MAX_CHAR);
      directory_listing[0] = '\0';

      index_listing = (char*) malloc(sizeof(char)*DIRECTORY_LISTING_MAX_CHAR);
      index_listing[0] = '\0';

      dir_listing = (char*) malloc(sizeof(char)*DIRECTORY_LISTING_MAX_CHAR);
      dir_listing[0] = '\0';

      // stores underlying info of files and sub_directories of directory_path
      struct dirent* underlying_file = NULL;

      int cx = snprintf(directory_listing,1013,index_hdr,rf,rf);

      // iterate through all of the  underlying files of directory_path
      while((underlying_file = readdir(path)) != NULL)
      {
	  // Check for index.html
	  if (strcmp(underlying_file->d_name, "index.html") == 0) {
		  (*flag)++; // Flag on
		  strcat(index_listing, underlying_file->d_name);
		  closedir(path);
		  return index_listing;
	  }

	  // Check for dir
          if (strcmp(underlying_file->d_name, "dir") == 0) {
                  strcat(dir_listing, underlying_file->d_name);
		  strcat(dir_listing, "/");
		  cx += snprintf(directory_listing+cx,1013-cx,index_body,dir_listing,underlying_file->d_name);
          }
	  else {
	  printf("%s\n", underlying_file->d_name);
	  cx += snprintf(directory_listing+cx,1013-cx,index_body,underlying_file->d_name,underlying_file->d_name);
	  }
	  //strcat(directory_listing, underlying_file->d_name);
          //strcat(directory_listing, "\n");
      }

      snprintf(directory_listing+cx,1013-cx,index_ftr);
      
      closedir(path);
  }

  return directory_listing;
}
//------------------------------------------------------------------
/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char* parseRequest(char* request) {
  //assume file paths are no more than 256 bytes + 1 for null. 
  char *buffer = malloc(sizeof(char)*257);
  memset(buffer, 0, 257);
  
  if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 

  sscanf(request, "GET %s HTTP/1.", buffer);
  return buffer; 
}
//------------------------------------------------------------------
void serve_request(int client_fd, char* directory_path){
  int read_fd;
  int bytes_read;
  int file_offset = 0;
  char client_buf[4096];
  char send_buf[4096];
  char filename[4096];
  char filename404[4096]; // Holds WWW/404.html
  char filenamedir[4096]; // Holds WWW/[dir]/[file]
  char * requested_file;
  char * combined_file; // Attaches WWW
  memset(client_buf,0,4096);
  memset(filename,0,4096);
  while(1){

    file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
    if(strstr(client_buf,"\r\n\r\n"))
      break;
  }
  requested_file = parseRequest(client_buf);
 
  strncpy(&filename[0],directory_path,4096);
  int path_length = strlen(directory_path);
  strncpy(&filename[path_length],requested_file,4096 - path_length);
  int file_length = strlen(requested_file);
  combined_file = filename;
  int combined_length = strlen(combined_file);

  // Check if file exists
  struct stat file_stat;
  if (stat(combined_file, &file_stat) != 0) {
	  // 404 Not found
	  printf("%s does not exist!\n", combined_file);
	  send(client_fd,not_found_str,strlen(not_found_str),0);

	  strncpy(&filename404[0],directory_path,4096);
	  strncpy(&filename404[path_length],"/404.html",4096 - path_length);

	  read_fd = open(filename404,0,0);

  	  while(1) {
    	    bytes_read = read(read_fd,send_buf,4096);
            if(bytes_read <= 0)
              break;

            send(client_fd,send_buf,bytes_read,0);
  	  }	
          close(read_fd);
          close(client_fd);
          return;
  }

  // Check if file is a directory
  if (S_ISDIR(file_stat.st_mode)) {
	  printf("%s is a directory\n", combined_file);
	  int flag = 0;
	  int *f = &flag;
	  char* directory_contents_str = get_directory_contents(combined_file, f,requested_file);
	  // index.html
	  if (flag == 1) {
	    send(client_fd,request_str,strlen(request_str),0);
	    strncpy(&filename[combined_length],directory_contents_str,4096 - combined_length);
	  }
	  // listing
	  else {
            send(client_fd,request_str,strlen(request_str),0);
	    send(client_fd,directory_contents_str,strlen(directory_contents_str),0);
            close(client_fd);
            return;
	  }
  }
  else {
	  printf("%s is NOT a directory!\n", combined_file);
	  if (strstr(combined_file, ".html")) {
		printf("html\n");
	  	send(client_fd,request_str,strlen(request_str),0);
	  }
	  else if (strstr(combined_file, ".pdf")) {
		printf("pdf\n");
                send(client_fd,pdf_str,strlen(pdf_str),0);
          }
	  else if (strstr(combined_file, ".gif")) {
		printf("gif\n");
                send(client_fd,gif_str,strlen(gif_str),0);
          }
	  else if (strstr(combined_file, ".png")) {
		printf("png\n");
                send(client_fd,png_str,strlen(png_str),0);
          }
	  else if (strstr(combined_file, ".jpg")) {
		printf("jpg\n");
                send(client_fd,jpg_str,strlen(jpg_str),0);
          }
	  else {
		printf("text\n");
	  	send(client_fd,text_str,strlen(text_str),0);
	  }
  }

  // take requested_file, add a . to beginning, open that file
  read_fd = open(filename,0,0);

  while(1){
    bytes_read = read(read_fd,send_buf,4096);
    if(bytes_read <= 0)
      break;

    send(client_fd,send_buf,bytes_read,0);
  }
  close(read_fd);
  close(client_fd);
  return;
}
//------------------------------------------------------------------
struct thread_arg {
	int thread_number;
	int sock;
};
//------------------------------------------------------------------
// Thread routine
void *thread(void *argument_value) {
	struct thread_arg *my_argument = (struct thread_arg *) argument_value;
	//pthread_detach(pthread_self());
	serve_request(my_argument->sock, william);
	close(my_argument->sock);
	return NULL;
}
//------------------------------------------------------------------
/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
    /* For checking return values. */
    int retval;

    // Check for enough arguments
    if (argc != 3) {
	    fprintf(stderr, "Arguments to Program not correct. Exiting.\n");
	    exit(-1);
    }

    // Check if file exists
    struct stat file_stat;
    if (stat(argv[2], &file_stat) != 0) {
	    printf("%s does not exist!  Program exiting\n", argv[2]);
	    exit(-1);
    }

    // Check if file is a directory
    if (S_ISDIR(file_stat.st_mode)) {
    	//printf("%s is a directory\n", argv[2]);
    } 
    else {
    	printf("%s is NOT a directory!  Program exiting\n", argv[2]);
    	exit(-1);
    }

    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);

    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
    
   
    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces

    
    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if(retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

    int num_threads, i;
    num_threads = 50000;
    struct thread_arg *arguments =
	    malloc(num_threads * sizeof(struct thread_arg));
    if (arguments == NULL) {
	    printf("malloc() failed\n");
	    exit(1);
    }

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
	    printf("malloc() failed\n");
	    exit(1);
    }

    i = -1;

    william = argv[2]; // Threading

    while(1) {
        /* Declare a socket for the client connection. */
        int sock;
        char buffer[256];

        /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr); 

        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
	if(sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

	i++;
	arguments[i].thread_number = i;
	arguments[i].sock = sock;
	retval = pthread_create(&threads[i], NULL,
			thread, (void *) &arguments[i]);
	if (retval) {
		printf("pthread_create() failed\n");
		exit(1);
	}

	retval = pthread_join(threads[i], NULL);
	if (retval) {
		printf("pthread_join() failed\n");
		exit(1);
	}

        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */
        //serve_request(sock, argv[2]);

        /* Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it. */
        //close(sock);
    }
    free(arguments);
    free(threads);
    close(server_sock);
}
