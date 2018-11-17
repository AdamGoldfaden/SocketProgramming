all:
	g++ -o client client.cpp
	g++ -o awsoutput aws.cpp 
	g++ -o serverAoutput serverA.cpp 
	g++ -o serverBoutput serverB.cpp 
	g++ -o serverCoutput serverC.cpp 
	g++ -o monitoroutput monitor.cpp 
	
serverA:
	./serverAoutput
serverB:
	./serverBoutput
serverC:
	./serverCoutput
aws:
	./awsoutput
monitor:
	./monitoroutput
