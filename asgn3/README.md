# Instructions to run code
	Type "make" into a terminal. This will compile the code.
	./httpserver to run the program.
	Arguments accepted by httpserver: a hostname and a port number.
	A host name is required, but if no port number is given, the default port will be 80.
	The flags accepted by httpserver are: -N, -a and -l
	-N must be followed by a number which will indicate the number of threads
	-a must be followed by a file name which will indicate which file to use as a mapping file. **This flag is required.**
	-l must be followed by a file name which will indicate which file to write logs into.
	
	
# Known issues
	Program works as expected during manual testing but does not pass gitlab tests.
	Specific tests that I believe I should pass because of manual testing:
		Test 7 - Test if server can resolve a short alias name
		Test 8 - Test if server can resolve a long alias name
		Test 9 - Test if server can have multiple alias's for a single file
		Test 11 - Test if server can detect an invalid PATCH request for a name that does not exist (404)
		
	For tests 7,8,9, I believe my tests should pass because I am able to add aliases for a resource using PATCH and resolve
	them using GET and PUT commands. I am also able to have multiple aliases for a single file in my mapping file and all 
	of the aliases work when using GET and PUT.
	
	For test 11, the only cases I can think of are when sending a PATCH request for a file that does not exist in the server 
	and an alias that does not exist in the mapping file. During manual testing, my server returns 404 for both cases.