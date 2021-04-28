# CS537-Web-Server
Multi-threaded webserver with producer-consumer model.


In this assignment, you will be developing a real, working web server. To simplify this project, we are providing you with the code for a very basic web server. 
This basic web server operates with only a single thread.  One of your main tasks is to make the web server multi-threaded so that it is more efficient.   
Your other main task is to create a shared memory segment which the threads can write to in order to export statistics that can be read by another process.

This project can be completed with a project partner.  

Objectives
To create and synchronize threads using pthreads, locks, and condition variables.  
To synchronize accesses to a shared buffer with threads in producer/consumer relationships.
To understand the basics of a web server.
To use shared memory across cooperating processes.  
To catch signals (such at SIGINT) with a signal handler.
